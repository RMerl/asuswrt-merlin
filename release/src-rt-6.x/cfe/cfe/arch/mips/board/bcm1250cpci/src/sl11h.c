/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM1250CPCI USB Driver					File: sl11h.c
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

//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
//#include <stdarg.h>
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_timer.h"
#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_smbus.h"
#include "bcm1250cpci.h"
#include "sl11h.h"

#define SBWRITEUSB(csr,val)   *((volatile unsigned char *) PHYS_TO_K1(csr)) = (val)
#define SBREADUSB(csr)      (*((volatile unsigned char *) PHYS_TO_K1(csr)))


unsigned short crc5(unsigned short f);
unsigned int usbXfer(unsigned char pid, unsigned short crc, unsigned int len, unsigned char *buf);
short ep0Xfer(PSetupPKG setup, unsigned char *pData);
short VendorCmd(unsigned char bReq,unsigned char bCmd,unsigned short wValue,unsigned short wIndex,unsigned short wLen,unsigned char* pData);
short GetDesc(unsigned short wValue, unsigned int cDesc, void * desc);
short SetAddress(unsigned short addr);
short uDataRead(unsigned int cData,void * pData);
short uDataWrite(unsigned int cData,void * pData);
short uCloseUsbDev(void);
int speed_detect(void);


/*--------------------------------------------------------------------------
 * Byte Read from SL11
 * Input:   address = address of SL11 register or memory
 * Input:   port    = usb port
 * Output:  return data 
 *--------------------------------------------------------------------------
 */
unsigned char SL11Read(unsigned char address)
{  
	if (address > 0xff) {
		printf("Invalid register access, register %x\n");
		return (0x0);
	}
//	printf("Address = %08X\n",PHYS_TO_K1(USBCTL_PHYS));
    SBWRITEUSB(USBCTL_PHYS,address);
//	printf("Address = %08X\n",PHYS_TO_K1(USBCTL_PHYS + A0_OFFSET));
    return(SBREADUSB(USBCTL_PHYS + A0_OFFSET));
}

/*--------------------------------------------------------------------------
 * Byte Write to SL11
 * Input:   address = address of SL11 register or memory, d = data
 * Input:   port    = usb port
 * Input:   data    = data to output
 *--------------------------------------------------------------------------
 */
void SL11Write(unsigned char address, unsigned char data)
{  
	if (address > 0xff) {
		printf("Invalid register access, register %x\n");
	}
    SBWRITEUSB(USBCTL_PHYS,address);
    SBWRITEUSB(USBCTL_PHYS + A0_OFFSET,data);
}

/*--------------------------------------------------------------------------
 * Buffer Read from SL11
 * Input:   address    = address of SL11 register or memory
 *          byte_count = byte count
 *          buffer     = char buffer
 * Output:  None
 *--------------------------------------------------------------------------
 */
void SL11BufRead(short address,unsigned char *buffer, short byte_count)
{	
   if (byte_count <= 0) return;
    SBWRITEUSB(USBCTL_PHYS,address);
    while (byte_count--) *buffer++ = SBREADUSB(USBCTL_PHYS + A0_OFFSET);
}

/*--------------------------------------------------------------------------
 * Buffer Write  to SL11
 * Input:  address    = address of SL11 register or memory
 *         byte_count = byte count
 *         buffer     = char buffer
 *--------------------------------------------------------------------------
 */
void SL11BufWrite(short address, unsigned char *buffer, short byte_count)
{	
   if(byte_count <= 0) return;
    SBWRITEUSB(USBCTL_PHYS,address);
    while (byte_count--)  SBWRITEUSB(USBCTL_PHYS + A0_OFFSET,*buffer++);

}

//--------------------------------------------------------------------------
// Delayms  mili second
//--------------------------------------------------------------------------
//void Delayms(unsigned int tt)
//{  
//   unsigned int time;
//   time=GetTickCount();
//   while(GetTickCount() < time+tt);
//}

/****************************************************************************
 * Full-speed and slow-speed detect
 ****************************************************************************/
int speed_detect(void)
{
    int i=0;
    int full_speed;

    full_speed = 0xffff;
    SL11Write(IntEna, 0x63);            // USBA/B, Insert/Remove,USBRest/Resume.
    SL11Write(cSOFcnt, 0xae);           // Set SOF high counter, no change D+/D-
//    SL11Write(CtrlReg, 0x48);           // Setup Normal Operation
    SL11Write(CtrlReg, 0x08);           // Setup Normal Operation
    SL11Write(CtrlReg, (unsigned char)i);        // Disable USB transfer operation and SOF
    SL11Write(cSOFcnt, 0xae);           // Set SOF high counter, no change D+/D-
//    SL11Write(CtrlReg, 0x48);           // Clear SL811H mode and setup normal operation
    SL11Write(CtrlReg, 0x08);           // Clear SL811H mode and setup normal operation
    cfe_sleep(1);                       // Sleep 10 ms
    SL11Write(CtrlReg, 0);	            // Disable USB transfer operation and SOF
    cfe_sleep(1);                       // Sleep 10 ms  (original file 1ms)
    SL11Write(IntStatus,0xff);
    i =  SL11Read(IntStatus);	        // Read Interrupt Status

	if(i & 0x40) {
        printf("Device is Removed\n");
        return 0;
	}

    if ((i & 0x80) == 0) {              // Checking full or slow speed
        printf("Slow Speed is detected %x\n", i);
        SL11Write(cSOFcnt,0xee);        // Set up Master and Slow Speed direct and SOF cnt high=0x2e
        SL11Write(cDATASet,0xe0);       // SOF Counter Low = 0xe0; 1ms interval
        SL11Write(CtrlReg,0x21);        // Setup 6MHz and EOP enable
        full_speed = 0;
    } else {   
        printf("Full Speed is detected %x\n", i);
        SL11Write(cSOFcnt,0xae);        // Set up Master and Slow Speed direct and SOF cnt high=0x2e
        SL11Write(cDATASet,0xe0);       // SOF Counter Low = 0xe0; 1ms interval
        SL11Write(CtrlReg,0x05);        // Setup 48MHz and SOF enable
    }
    SL11Write(EP0Status,  0x50);
    SL11Write(EP0Counter, 0);
    SL11Write(EP0Control, 0x01);        // start generate SOF or EOP
    cfe_sleep(3);                       // Sleep 30 ms
//    Delayms(25);                        // Hub required approx. 24.1mS
    return 0;    
}



/*-------------------------------------------------------------------------
 * SL11 Memory test
 * Input:   None
 * Output:  return 0 = no error
 *-------------------------------------------------------------------------
 */
int SL11HMemTest(void)
{ 
    int errors = 0;
    int i;

    /* Write Data to buffer */
    for (i=EP0Buf; i<sMemSize; i++)
        SL11Write((unsigned char)i,(unsigned char)i);
    
    /* Verify Data in buffer using byte checks */
    for (i=EP0Buf; i<sMemSize; i++) {
        if ((unsigned char)i != SL11Read((unsigned char)i)) 
            errors++;
        SL11Write ((unsigned char)i,(unsigned char)~i);
        if ((unsigned char)~i != SL11Read((unsigned char)i))
            errors++;
    }

    /* Write data to buffer using auto increment mode */
    SBWRITEUSB(USBCTL_PHYS,EP0Buf);
    for (i=EP0Buf; i<sMemSize; i++)
        SBWRITEUSB(USBCTL_PHYS + A0_OFFSET,i);

    /* Verify data in buffer using auto increment mode */
    SBWRITEUSB(USBCTL_PHYS,EP0Buf);
    for (i=EP0Buf; i<sMemSize; i++) {
        if ((unsigned char)i != SBREADUSB(USBCTL_PHYS + A0_OFFSET))
        //(unsigned char)*(unsigned char *)(PHYS_TO_K1(USBCTL_PHYS + A0_OFFSET)))
            errors++;
    }
    
    /* Clear memory in buffer using auto increment mode */
    SBWRITEUSB(USBCTL_PHYS,EP0Buf);
    for (i=EP0Buf; i<sMemSize; i++)
       SBWRITEUSB(USBCTL_PHYS + A0_OFFSET,0);
    return errors;
}

/*-------------------------------------------------------------------------
 * SL11 Reset
 * Input:   None
 * Output:  None
 *-------------------------------------------------------------------------
 */
void USBReset(void)
{

	unsigned char tmp;
	tmp = SL11Read(CtrlReg);
	SL11Write(CtrlReg, tmp | 0x08);
    cfe_sleep(CFE_HZ / 4);              // Delay 250 ms
	SL11Write(CtrlReg, tmp | 0x18);
    cfe_sleep(CFE_HZ / 6);              // Delay 166 ms
	SL11Write(CtrlReg, tmp | 0x08);
    cfe_sleep(CFE_HZ / 10);             // Delay 100ms 
	speed_detect();
//	SL11Write(CtrlReg, tmp & 0xBF);


//    SL11Write(CtrlReg, 8);          /* Setup USB Reset */
//    cfe_sleep(CFE_HZ / 10);         /* Delay 100ms */
//    SL11Write(CtrlReg, 0);          /* Enable USB */
//    cfe_sleep(CFE_HZ / 10);         /* Delay 100ms */
//    SL11Write(0x0d, 0xff);
}

/* From original */
//    BYTE tmp;
//    tmp = SL11Read(CtrlReg);                
//    SL11Write(CtrlReg, (BYTE)(tmp | 8));    // Setup USB Reset
//    Delayms(20);
//    SL11Write(CtrlReg, tmp);               // enable USB
//    Delayms(250);
//    return 0;


/****************************************************************************
 * SL811H mode Hardware SOF/EOP generation
 ****************************************************************************/

//--------------------------------------------------------------------------
// Sending EOP packet
//   -- (Not used in SL811H)
//--------------------------------------------------------------------------

/*--------------------------------------------------------------------------
 * CRC5
 *--------------------------------------------------------------------------
 */
unsigned short crc5(unsigned short f)
{  unsigned short i,seed=0x1f,t=f,flag;
   for(i=0;i<11;i++)
   {    flag=(t^seed)&1;
		seed>>=1;
		t>>=1;
		if(flag) seed^=0x14;
   }
   seed^=0x1f;
   return((f&0x7ff)|((seed&0x1f)<<11));
}

/*--------------------------------------------------------------------------
 * Global variable
 *--------------------------------------------------------------------------
 */
#define hUsbAddr 2
unsigned short  EP0CRC=0;     /* Endpoint 0: addr + (ep0<<7)             */
unsigned short  ReadCRC=0;    /* current Read Endpoint: addr + (epx<<7)  */
unsigned short  WriteCRC=0;   /* current Write Endpoint: addr + (epx<<7) */
unsigned char  hUsbNum=0;    /* Device address & number of USB */
unsigned char  ep0Len=0;

/*--------------------------------------------------------------------------
 * usbXfer:
 * return 0 on Success
 * Status bit for register EP0Status
 *   if (result&0x40)    Device return NAKs, forever
 *   if (result&0x02)    Device Error Bit
 *   if (result&0x04)    Device Time out
 *   if (result&0x80)    Device return STALL
 *--------------------------------------------------------------------------
 */
unsigned int usbXfer(unsigned char pid, unsigned short crc, unsigned int len, unsigned char *buf)
{
    unsigned int time;
    short result=0, retry=0, bLen, rLen;
    unsigned char  Cmd=DATA0_RD, sMem=0x40;
        
    SL11Write(0x40,pid);                            /* PID         */
    SL11BufWrite(0x41,(unsigned char*)&crc,2);      /* 2 bytes CRC */
    if (len>=(unsigned int)ep0Len) {
        /* setup ping pong buffer */
		printf("Setting up ping pong\n");
        SL11Write(0x90,pid);                        /* PID         */
        SL11BufWrite(0x91,(unsigned char*)&crc,2);  /* 2 bytes CRC */
        rLen = ep0Len;
    }
    else
        rLen = (short)len;

    if (pid != PID_IN) {
		printf("Not PID IN\n");
        SL11BufWrite(0x43,buf,rLen);                /* DATA to SL11 Memory */
        Cmd = DATA0_WR;
    }
        SL11Write(EP0XferLen,(unsigned char)(0x03+rLen));
        SL11Write(EP0Address,sMem);
	printf("Setting up transfer\n");
    do
    {       
        SL11Write(0x0d,0xff);          				/* Clear Interrupts */
        SL11Write(EP0Status,0);
        SL11Write(EP0Control,Cmd);                  /* Enable ARM     */
    XferLoop:
		printf(".");
        bLen = ((unsigned int)(len - rLen) >= (unsigned int)ep0Len) ? (unsigned char)ep0Len : (unsigned char)(len - rLen);
        sMem = (Cmd & 0x40) ? 0x40 : 0x90;          /* next ping pong buffer */
        if ((pid != PID_IN) && !retry)              /* Write ahead for ping pong buffer */
            SL11BufWrite((unsigned char)(sMem + 3),buf + rLen,bLen);
        
		printf("Setting up timer\n");
        TIMER_SET(time,TIMEOUT);
        while(!(SL11Read(0x0d) & 1)) {               /* check interrupt bit  */
            if(TIMER_EXPIRED(time)) {
                printf("TimeOut, Abort\n"); 
                USBReset();
                return -1;
            }
		POLL();
		}
        
		printf("Fell out of timer\n");
//        time=GetTickCount();
//        while(!(SL11Read(0x0d) & 1))                /* check interrupt bit  */
//            if(GetTickCount() > time+TIMEOUT) {
//                printf("TimeOut, Abort\n"); 
//                USBReset();
//                return -1;
//            }
        result=SL11Read(EP0Status);
        if (result & 1) {                           /* ACK bit */
            retry = 0;
            Cmd ^= 0x40;                            /* toggle DATA0/DATA1 */
            // rLen = (unsigned char)(SL11Read(EP0XferLen)-SL11Read(EP0Counter)-3);
            len -= rLen;                            /* rLen = actual read/write */
            if (bLen) {
                SL11Write(EP0XferLen,(unsigned char)(0x03 + bLen)); 
                SL11Write(EP0Address,sMem);         /* data addr  */
                SL11Write(0x0d,0xff);               // SL11Write(EP0Counter,0x00);
                SL11Write(EP0Control,Cmd);          /* Enable ARM */
            }
            if (pid==PID_IN)
                SL11BufRead((short)((Cmd & 0x40) ? 0x43 : 0x93),buf,rLen);
            rLen = bLen;
            if (result & 0x20)                      // payload should be check
                break;
            buf += rLen;
            if (len) goto XferLoop;
            else break;                             // on SUCCESS
        }
        if (result & 0x80) break;                   // STALL
    } while (retry-- < MAX_RETRY);

    if (result & 4) USBReset();
    printf("PID=%x Result = %x len=%d\n",pid, result,len);    
    if (result & 1) return len;
    return -1;
}

/***********************************************************************
 * ep0Xfer
 * Description :	Endpoint 0 cmd block
 * Parameters  :	hUsbDev
 *         bReq     Read/Write Control
 *         bCmd     Command Opcode
 *         wValue   User 1st Parameter 
 *         wIndex   User 2nd Parameter 
 *         wLen     Length for pData
 *         pData    User's data buffer pointer
 * Return      :	TRUE means successful, else FAIL
 **********************************************************************/
short ep0Xfer(PSetupPKG setup, unsigned char *pData)
{
    unsigned short pid=PID_IN, wLen = setup->wLength;
    
    if (usbXfer(PID_SETUP,EP0CRC,8,(unsigned char*)setup)==-1) return FALSE;
	printf("Survived First\n");
    if (wLen) {
        if (setup->bmRequest&0x80) {
            pid=PID_OUT;
            wLen = (unsigned short)usbXfer(PID_IN,EP0CRC,wLen,pData);
        } else {
            wLen=(unsigned short)usbXfer(PID_OUT,EP0CRC,wLen,pData);
        }
    }
    usbXfer((unsigned char)pid,EP0CRC,0,NULL);
//    return (wLen >= 0);
    return(1);
    
}

short VendorCmd(unsigned char bReq,unsigned char bCmd,unsigned short wValue,unsigned short wIndex,unsigned short wLen,unsigned char* pData)
{ 
    SetupPKG setup;
	setup.bmRequest = bReq;
	setup.bRequest  = bCmd;
	setup.wValue    = wValue;
	setup.wIndex    = wIndex;
	setup.wLength   = wLen;
    return (ep0Xfer(&setup, pData));
}

/*--------------------------------------------------------------------------
 * GetDesc
 * require:  EP0CRC
 *--------------------------------------------------------------------------
 */
short GetDesc(unsigned short wValue, unsigned int cDesc, void * desc)
{ 
    return VendorCmd(0x80, 6, wValue, 0, (unsigned short)cDesc, desc);
}

short SetAddress(unsigned short addr)
{
    return VendorCmd(0x00, 5, addr, 0, 0, NULL);
}

/***************************************************************************/
short uDataRead(unsigned int cData,void * pData)
{
    return usbXfer(PID_IN,ReadCRC,cData,(unsigned char*)pData)==0;
}

/***************************************************************************/
short uDataWrite(unsigned int cData,void * pData)
{
    return usbXfer(PID_OUT,WriteCRC,cData,(unsigned char*)pData)==0;
}

/***************************************************************************/
#define TMPSIZE 64                          /* should use small buffer for SL11R */
static unsigned char tmp[TMPSIZE];          /* temp buffer */
short uFindUsbDev(unsigned short UsbProd)
{  
    pDevDesc  pDev;
    pConfDesc pConf;
    pIntfDesc pIntf;
    pEPDesc   pEP;
    int i;

    if(!hUsbNum) { 
        USBReset();
        EP0CRC = crc5(0);                   /* address = 0, endpoint = 0 */
        ep0Len = 8;
        printf("Device 0: .. ");

        pDev = (pDevDesc) & tmp[0];
		printf("Getting Descriptor\n");
        if (!GetDesc(DEVICE << 8,0x40,tmp)) return FALSE;
		printf("Setting Address\n");
        if (!SetAddress(hUsbAddr)) return FALSE;

        printf("len=%02x ep0Len=%d\n",pDev->bLength,pDev->bMaxPacketSize0);
        ep0Len = (pDev->bMaxPacketSize0&0xff)?pDev->bMaxPacketSize0:8;

        printf("Device 2: ..");
        EP0CRC = crc5((unsigned short)(hUsbAddr + (0 << 7)));  /* new Endpoint 0 CRC */

        if (!GetDesc(DEVICE << 8,0x20,tmp)) return FALSE;
        printf("Vendor: %04x Product: %04x\n",pDev->idVendor,pDev->idProduct);

        if (!GetDesc(CONFIGURATION << 8,64,tmp)) return FALSE;
        pConf = (pConfDesc) & tmp[0];
        printf("Len=%d, type: %02x\n", pConf->wLength, pConf->bType);
        if (!GetDesc(CONFIGURATION << 8,TMPSIZE,tmp)) return FALSE; 

        pIntf = (pIntfDesc) & tmp[tmp[0]];          /* point to Interface Desc */
        pEP = (pEPDesc)&tmp[tmp[0]+pIntf->bLength]; /* point to EP Desc */
        printf("Number of Endpoint = %d\n",pIntf->bEndPoints);
        ep0Len = 0x40;

        if (pIntf->bEndPoints) hUsbNum = 1;
        else  return FALSE;

        for (i=1; i <= pIntf->bEndPoints; i++) {
            printf("EndPoint Addr %02x Attr %02x wLength=%04x\n",pEP->bEPAdd, pEP->bAttr, pEP->wPayLoad);
            if (pEP->bAttr == 1) {
                printf("Iso Endpoint doesnot supported\n");
                return FALSE;
            }
            if (pEP->wPayLoad != 0x40) {
                printf("Endpoint Payload requires = 0x40\n");
                return FALSE;
            }
          if (pEP->bEPAdd&0x80)
             ReadCRC = crc5((unsigned short)(hUsbAddr + (i << 7))); /* Read Endpoint CRC */
          else
             WriteCRC=crc5((unsigned short)(hUsbAddr + (i << 7)));  /* Write Endpoint CRC */
          (unsigned char*)pEP  += pEP->bLength;
        }

    }
    return TRUE;
}


short uCloseUsbDev(void)
{
    hUsbNum = 0;
    return TRUE;
}

/*--------------------------------------------------------------------------
 * main Program
 *--------------------------------------------------------------------------
 */
 
/*

#define IMSIZE  256*1024
unsigned char ibuf[IMSIZE];
int main()
{
    int data;

    for (data=0; data<IMSIZE; data++)
        ibuf[data] = data;

    if (SL11HMemTest())
        {
        printf("SL11H Memory test fail or SL11H Device not Install\n");
        exit(1);
        }
    else
        printf("SL11H Memory test PASS\n");

    while (1)
        {   
        printf("Hit any key to contine\n");

        switch (getch())
            {
            case 'g': 
                hUsbNum = 0;
                if (uFindUsbDev(0x4ce)) 
                    printf("FindUsbDev: SUCCESS\n");
                else
                    printf("FindUsbDev: FAIL\n");
                break;
            
            case 'e':
                exit(0);
                break;

            case 'r': 
                uVendorCmdRead(0x48, 0xc00, 0xc00, 64, tmp);
                printf("Return data %04x\n",*(unsigned short*)&tmp[0]);
                break;

            case 'w':
                printf("Get data => "); scanf("%x",&data); 
                printf("data enter = %x\n",data);
                uVendorCmdWrite(0x47, 0xc00, (unsigned short)data, 0, NULL);
                break;

            case 'd':
                printf("Get Read data size => "); scanf("%x",&data); 
                if (data>IMSIZE) data = IMSIZE;   
                printf("data enter = %x\n",data);
                uVendorCmdWrite(0x4a,(unsigned short)(data>>16), (unsigned short)data, 0, NULL);
                printf("Status = %x\n",uDataRead(data, ibuf));
                for (data=0; data<12; data++)
                    printf("%02x ",ibuf[data]); printf("\n");
                break;

            case 'f':
                printf("Get Write data size => "); scanf("%x",&data); 
                if (data>IMSIZE) data = IMSIZE;   
                printf("data enter = %x\n",data);
                uVendorCmdWrite(0x4b,(unsigned short)(data>>16), (unsigned short)data, 0, NULL);
                printf("Status = %x\n",uDataWrite(data, ibuf));
                break;
            }
        }
}

*/ 
