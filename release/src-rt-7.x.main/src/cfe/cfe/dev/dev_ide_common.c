/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Generic IDE disk driver 			File: dev_ide_common.c
    *  
    *  This is a simple driver for IDE hard disks.   The mechanics
    *  of talking to the I/O ports are abstracted sufficiently to make
    *  this driver usable for various bus interfaces.
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
#include "lib_string.h"
#include "cfe_timer.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"

#include "dev_ide_common.h"

#include "dev_ide.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define DISK_MASTER	0
#define DISK_SLAVE	1

#define IDE_WRITEREG8(ide,reg,val)     IDEDISP_WRITEREG8(ide->idecommon_dispatch,reg,val)
#define IDE_WRITEREG16(ide,reg,val)    IDEDISP_WRITEREG8(ide->idecommon_dispatch,reg,val)
#define IDE_WRITEBUF(ide,reg,buf,len)  IDEDISP_WRITEBUF(ide->idecommon_dispatch,reg,buf,len)
#define IDE_READREG8(ide,reg)          IDEDISP_READREG8(ide->idecommon_dispatch,reg)
#define IDE_READREG16(ide,reg)         IDEDISP_READREG16(ide->idecommon_dispatch,reg)
#define IDE_READBUF(ide,reg,buf,len)   IDEDISP_READBUF(ide->idecommon_dispatch,reg,buf,len)

#define GETWORD_LE(buf,wordidx) (((unsigned int) (buf)[(wordidx)*2]) + \
             (((unsigned int) (buf)[(wordidx)*2+1]) << 8))

#define _IDE_DEBUG_


static void idecommon_testdrq(idecommon_t *ide);

/*  *********************************************************************
    *  idecommon_sectorshift(size)
    *  
    *  Given a sector size, return log2(size).  We cheat; this is
    *  only needed for 2048 and 512-byte sectors.
    *  Explicitly using shifts and masks in sector number calculations
    *  helps on 32-bit-only platforms, since we probably won't need
    *  a helper library.
    *  
    *  Input parameters: 
    *  	   size - sector size
    *  	   
    *  Return value:
    *  	   # of bits to shift
    ********************************************************************* */

#define idecommon_sectorshift(size) (((size)==2048)?11:9)

/*  *********************************************************************
    *  idecommon_waitnotbusy(ide)
    *  
    *  Wait for an IDE device to report "not busy"
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   
    *  Return value:
    *  	   0 if ok, else -1 if timeout
    ********************************************************************* */

static int idecommon_waitnotbusy(idecommon_t *ide)
{
    int32_t timer;
    uint8_t status;

    TIMER_SET(timer,10*CFE_HZ);

    while (!TIMER_EXPIRED(timer)) {
	status = IDE_READREG8(ide,IDE_REG_STATUS);
	if (!(status & IDE_STS_BSY) && (status & IDE_STS_DRQ)) {
	    idecommon_testdrq(ide);
	    continue;
	    }
	if ((status & (IDE_STS_BSY | IDE_STS_DRQ )) == 0) return 0;
	POLL();
	}

#ifdef _IDE_DEBUG_
    xprintf("Device did not become unbusy\n");
#endif
    return -1;
}


/*  *********************************************************************
    *  idecommon_waitbusy(idx)
    *  
    *  Wait for an IDE disk to start processing a command, or at 
    *  least long enough to indicate that it is doing so.  
    *  The code below looks suspiciously like a timing loop. 
    *  unfortunately, that's what it is, determined empirically 
    *  for certain ATA flash cards.  Without this many reads to the
    *  ALTSTAT register, the PCMCIA controller deasserts the
    *  card detect pins briefly.  Anyone have any clues?
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   
    *  Return value:
    *  	   void
    ********************************************************************* */

static void idecommon_waitbusy(idecommon_t *ide)
{
    int idx;

    for (idx = 0; idx < 10; idx++) {
	IDE_READREG8(ide,IDE_REG_ALTSTAT);
	IDE_READREG8(ide,IDE_REG_ALTSTAT);
	IDE_READREG8(ide,IDE_REG_ALTSTAT);
	IDE_READREG8(ide,IDE_REG_ALTSTAT);
	}
}


/*  *********************************************************************
    *  idecommon_wait_drq(ide)
    *  
    *  Wait for the BUSY bit to be clear and the DRQ bit to be set.
    *  This is usually the indication that it's time to transfer data.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   0 if DRQ is set
    *  	   -1 if timeout occured
    ********************************************************************* */

static int idecommon_wait_drq(idecommon_t *ide)
{
    int32_t timer;
    uint8_t status;

    TIMER_SET(timer,10*CFE_HZ);

    while (!TIMER_EXPIRED(timer)) {
	POLL();
	status = IDE_READREG8(ide,IDE_REG_STATUS);
	if (!(status & IDE_STS_BSY) && (status & IDE_STS_ERR)) {
	    xprintf("Drive status: %02X error %02X\n",status,
		    IDE_READREG8(ide,IDE_REG_ERROR));
	    return -1;
	    }
	if (!(status & IDE_STS_BSY) && (status & IDE_STS_DRQ)) return 0;
	}

#ifdef _IDE_DEBUG_
    xprintf("Timeout waiting for disk\n");
#endif

    return -1;
}

/*  *********************************************************************
    *  idecommon_testdrq(ide)
    *  
    *  Debug routine.  Check the DRQ bit.  If it's set, it is not
    *  supposed to be, so transfer data until it clears and report
    *  how much we had to transfer.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifdef _IDE_DEBUG_
static void idecommon_testdrq(idecommon_t *ide)
{
    uint8_t status;
    uint16_t data;
    int idx;

    status = IDE_READREG8(ide,IDE_REG_STATUS);
    if (status & IDE_STS_DRQ) {
	xprintf("Error: DRQ should be zero\n");
	idx = 0;
	while (status & IDE_STS_DRQ) {
	    data = IDE_READREG16(ide,IDE_REG_DATA);
	    idx++;
	    status = IDE_READREG8(ide,IDE_REG_STATUS);
	    }
	xprintf("Had to read data %d times to clear DRQ\n",idx);
	}
}
#else
#define idecommon_testdrq(ide)
#endif


/*  *********************************************************************
    *  idecommon_dumpregs(ide)
    *  
    *  Dump out the IDE registers. (debug routine)
    *  
    *  Input parameters: 
    *  	   ide - ide disk
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void idecommon_dumpregs(idecommon_t *ide)
{
}


/*  *********************************************************************
    *  idecommon_reset(ide)
    *  
    *  Reset the IDE interface.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   
    *  Return value:
    *  	   0 if ok, else -1 if a timeout occured
    ********************************************************************* */

static int idecommon_reset(idecommon_t *ide)
{
    return 0;
}

/*  *********************************************************************
    *  idecommon_identify(ide,buffer)
    *  
    *  Execute an IDENTIFY command to get information about the disk.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   buffer - pointer to 512 byte buffer
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int idecommon_identify(idecommon_t *ide,unsigned char *buffer)
{

    /* Device Select Protocol; see ATAPI-4 sect 9.6 */
      
    if (idecommon_waitnotbusy(ide) < 0) return -1;
    IDE_WRITEREG8(ide,IDE_REG_DRVHD,(ide->idecommon_unit<<4)|0);
    if (idecommon_waitnotbusy(ide) < 0) return -1;

    /* Set device registers */

    IDE_WRITEREG8(ide,IDE_REG_CYLLSB,0);
    IDE_WRITEREG8(ide,IDE_REG_CYLMSB,0);
    IDE_WRITEREG8(ide,IDE_REG_SECNUM,1);
    IDE_WRITEREG8(ide,IDE_REG_SECCNT,1);

    idecommon_testdrq(ide);

    /* Issue command, then read ALT STATUS (9.7) */

    if (ide->idecommon_atapi) {
	IDE_WRITEREG8(ide,IDE_REG_COMMAND,IDE_CMD_ATAPI_IDENTIFY);
	}
    else {
	IDE_WRITEREG8(ide,IDE_REG_COMMAND,IDE_CMD_DRIVE_INFO);
	}

    IDE_READREG8(ide,IDE_REG_ALTSTAT); 
    idecommon_waitbusy(ide);		/* should not be necessary */

    /* Wait BSY=0 && DRQ=1, then read buffer, see sect 9.7 */

    if (idecommon_wait_drq(ide) < 0) return -1;
    IDE_READBUF(ide,IDE_REG_DATA,buffer,DISK_SECTORSIZE);

    idecommon_testdrq(ide);

    return 0;
}

/*  *********************************************************************
    *  idecommon_packet(ide,packet,pktlen,databuf,datalen)
    *  
    *  Process an IDE "packet" command, for ATAPI devices
    * 
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   packet,pktlen - command packet
    *  	   databuf,datalen - data buffer
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int idecommon_packet(idecommon_t *ide,
			uint8_t *packet,int pktlen,
			uint8_t *databuf,int datalen)
{
    uint8_t status;

    /*
     * Not valid on non-ATAPI disks
     */

    if (!ide->idecommon_atapi) return -1;

    /*
     * Set up the standard IDE registers for an ATAPI PACKET command
     */

    /* Device Select Protocol */
    if (idecommon_waitnotbusy(ide) < 0) return -1;
    IDE_WRITEREG8(ide,IDE_REG_DRVHD,(ide->idecommon_unit<<4));
    if (idecommon_waitnotbusy(ide) < 0) return -1;

    /* Device Registers */
    IDE_WRITEREG8(ide,IDE_REG_BCLSB,(datalen & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_BCMSB,((datalen >> 8) & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_SECNUM,0);
    IDE_WRITEREG8(ide,IDE_REG_SECCNT,0);
    IDE_WRITEREG8(ide,IDE_REG_COMMAND,IDE_CMD_ATAPI_PACKET);

    /*
     * Wait for the DRQ bit to indicate that we should send
     * the packet.
     */

    if (idecommon_wait_drq(ide) < 0) return -1;

    status = IDE_READREG8(ide,IDE_REG_IR);

    /*
     * Send the packet to the device
     */

    IDE_WRITEBUF(ide,IDE_REG_DATA,packet,pktlen);

    /*
     * Wait for BSY to be cleared and DRQ to be set.
     */

    if (idecommon_wait_drq(ide) < 0) return -1;
    status = IDE_READREG8(ide,IDE_REG_ALTSTAT);
    if (idecommon_wait_drq(ide) < 0) return -1;
    status = IDE_READREG8(ide,IDE_REG_IR);


    /*
     * Transfer data, if necessary.  The direction will depend
     * on what the drive says.  If this is a non-data command,
     * passing databuf == NULL can avoid all this.
     */

    if (databuf) {
	status = IDE_READREG8(ide,IDE_REG_IR);
	if (status & IDE_IR_CD) {
	    xprintf("Error: Command/data should be zero\n");
	    }

	if (status & IDE_IR_IO) {	/* from device (READ) */
	    IDE_READBUF(ide,IDE_REG_DATA,databuf,datalen);
	    }
	else {				/* to device (WRITE) */
	    IDE_WRITEBUF(ide,IDE_REG_DATA,databuf,datalen);
	    }

	}


    idecommon_testdrq(ide);

    return 0;

}


/*  *********************************************************************
    *  idecommon_request_sense(ide)
    *  
    *  Request sense data.  This also clears a UNIT_ATTENTION condition
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */
static int idecommon_request_sense(idecommon_t *ide)
{
    uint8_t cdb[12];
    uint8_t sensedata[32];
    int res;
    int numbytes;

    numbytes = sizeof(sensedata);

    cdb[0] = CDB_CMD_REQSENSE;
    cdb[1] = 0;
    cdb[2] = 0;
    cdb[3] = 0;
    cdb[4] = sizeof(sensedata);
    cdb[5] = 0;
    cdb[6] = 0;
    cdb[7] = 0;
    cdb[8] = 0;
    cdb[9] = 0;
    cdb[10] = 0;
    cdb[11] = 0;

    memset(sensedata,0,sizeof(sensedata));

    res = idecommon_packet(ide,cdb,sizeof(cdb),sensedata,numbytes);


    idecommon_testdrq(ide);

    return res;
}


/*  *********************************************************************
    *  idecommon_read_atapi(ide,lba,numsec,buffer)
    *  
    *  Read sector(s) from the device.  This version is for ATAPI devs.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   lba - logical block address
    *  	   numsec - number of sectors
    *  	   buffer - buffer address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int idecommon_read_atapi(idecommon_t *ide,uint64_t lba,
				int numsec,unsigned char *buffer)
{
    uint8_t cdb[12];
    int res = 0;
    int numbytes;
    int idx;

    numbytes = numsec << idecommon_sectorshift(ide->idecommon_sectorsize);

    cdb[0] = CDB_CMD_READ;
    cdb[1] = 0;
    cdb[2] = ((lba >> 24) & 0xFF);
    cdb[3] = ((lba >> 16) & 0xFF);
    cdb[4] = ((lba >> 8)  & 0xFF);
    cdb[5] = ((lba >> 0)  & 0xFF);
    cdb[6] = 0;
    cdb[7] = ((numsec >> 8) & 0xFF);
    cdb[8] = ((numsec >> 0) & 0xFF);
    cdb[9] = 0;
    cdb[10] = 0;
    cdb[11] = 0;

    for (idx = 0; idx < 4; idx++) {
	res = idecommon_packet(ide,cdb,sizeof(cdb),buffer,numbytes);
	if (res < 0) {
	    idecommon_request_sense(ide);
	    continue;
	    }
	break;
	}

    return res;
}


/*  *********************************************************************
    *  idecommon_write_atapi(ide,lba,numsec,buffer)
    *  
    *  Write sector(s) to the device.  This version is for ATAPI disks
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   lba - logical block address
    *  	   numsec - number of sectors
    *  	   buffer - buffer address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int idecommon_write_atapi(idecommon_t *ide,uint64_t lba,
				 int numsec,unsigned char *buffer)
{
    uint8_t cdb[12];
    int res;
    int numbytes;

    numbytes = numsec << idecommon_sectorshift(ide->idecommon_sectorsize);

    cdb[0] = CDB_CMD_WRITE;
    cdb[1] = 0;
    cdb[2] = ((lba >> 24) & 0xFF);
    cdb[3] = ((lba >> 16) & 0xFF);
    cdb[4] = ((lba >> 8)  & 0xFF);
    cdb[5] = ((lba >> 0)  & 0xFF);
    cdb[6] = 0;
    cdb[7] = ((numsec >> 8) & 0xFF);
    cdb[8] = ((numsec >> 0) & 0xFF);
    cdb[9] = 0;
    cdb[10] = 0;
    cdb[11] = 0;

    res = idecommon_packet(ide,cdb,sizeof(cdb),buffer,numbytes);

    return res;
}


/*  *********************************************************************
    *  idecommon_read_lba(ide,lba,numsec,buffer)
    *  
    *  Read sector(s) from the device.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   lba - logical block address
    *  	   numsec - number of sectors
    *  	   buffer - buffer address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int idecommon_read_lba(idecommon_t *ide,uint64_t lba,int numsec,unsigned char *buffer)
{
    int secidx;
    unsigned char *ptr;

    if (idecommon_waitnotbusy(ide) < 0) return -1;
    IDE_WRITEREG8(ide,IDE_REG_DRVHD,(ide->idecommon_unit<<4) | ((lba >> 24) & 0x0F) | 0x40);
    if (idecommon_waitnotbusy(ide) < 0) return -1;

    IDE_WRITEREG8(ide,IDE_REG_CYLMSB,((lba >> 16) & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_CYLLSB,((lba >> 8) & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_SECNUM,(lba & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_SECCNT,numsec);

    idecommon_testdrq(ide);

    IDE_WRITEREG8(ide,IDE_REG_COMMAND,IDE_CMD_READ);

    idecommon_waitbusy(ide);
    if (idecommon_wait_drq(ide) < 0) return -1;

    ptr = buffer;

    for (secidx = 0; secidx < numsec; secidx++) {
	IDE_READBUF(ide,IDE_REG_DATA,ptr,ide->idecommon_sectorsize);
	ptr += ide->idecommon_sectorsize;
	}

    idecommon_testdrq(ide);

    return 0;
}


/*  *********************************************************************
    *  idecommon_write_lba(ide,lba,numsec,buffer)
    *  
    *  Write sector(s) from the device.
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   lba - logical block address
    *  	   numsec - number of sectors
    *  	   buffer - buffer address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int idecommon_write_lba(idecommon_t *ide,uint64_t lba,int numsec,unsigned char *buffer)
{
    int secidx;
    uint8_t *ptr;

    if (idecommon_waitnotbusy(ide) < 0) return -1;
    IDE_WRITEREG8(ide,IDE_REG_DRVHD,(ide->idecommon_unit<<4) | ((lba >> 24) & 0x0F) | 0x40);
    if (idecommon_waitnotbusy(ide) < 0) return -1;

    IDE_WRITEREG8(ide,IDE_REG_CYLMSB,((lba >> 16) & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_CYLLSB,((lba >> 8) & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_SECNUM,(lba & 0xFF));
    IDE_WRITEREG8(ide,IDE_REG_SECCNT,numsec);

    IDE_WRITEREG8(ide,IDE_REG_COMMAND,IDE_CMD_WRITE);

    if (idecommon_wait_drq(ide) < 0) return -1;

    ptr = buffer;

    for (secidx = 0; secidx < numsec; secidx++) {
	IDE_WRITEBUF(ide,IDE_REG_DATA,ptr,ide->idecommon_sectorsize);
	ptr += ide->idecommon_sectorsize;
	}

    idecommon_testdrq(ide);

    return 0;
}


/*  *********************************************************************
    *  idecommon_diagnostic(ide)
    *  
    *  run the device diagnostics on the IDE device.  This also
    *  helps us determine if it's an IDE or ATAPI disk, since the
    *  diagnostic will leave a signature in the registers.
    *  
    *  Input parameters: 
    *  	   softc - IDE interface
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int idecommon_diagnostic(idecommon_t *softc)
{
    if (idecommon_waitnotbusy(softc) < 0) return -1;
    IDE_WRITEREG8(softc,IDE_REG_DRVHD,(softc->idecommon_unit<<4));
    if (idecommon_waitnotbusy(softc) < 0) return -1;

    IDE_WRITEREG8(softc,IDE_REG_COMMAND,IDE_CMD_DIAGNOSTIC);
    if (idecommon_waitnotbusy(softc) < 0) return -1;

    cfe_sleep(CFE_HZ/2);
    idecommon_dumpregs(softc);

    return 0;
}


/*  *********************************************************************
    *  idecommon_getmodel(buffer,model)
    *  
    *  Get the ASCII model name out of an IDE identify buffer.  some
    *  byte swapping is involved here.  The trailing blanks are trimmed.
    *  
    *  Input parameters: 
    *  	   buffer - 512-byte buffer from IDENTIFY command
    *  	   model - 41-byte string (max) for model name
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void idecommon_getmodel(uint8_t *buffer,char *model)
{
    uint16_t w;
    int idx;

    for (idx = 0; idx < 20; idx++) {
	w = GETWORD_LE(buffer,27+idx);
	model[idx*2] = w >> 8;
	model[idx*2+1] = w & 0xFF;
	}
    for (idx = 39; idx > 0; idx--) {
	if (model[idx] != ' ') {
	    model[idx+1] = '\0';
	    break;
	    }
	}

}

/*  *********************************************************************
    *  idecommon_devprobe(softc)
    *  
    *  Probe the IDE device, to determine if it's actually present
    *  or not.  If present, determine if it's IDE or ATAPI and
    *  get the device size.  Init our internal structures so we know
    *  how to talk to the device.
    * 
    *  Input parameters: 
    *  	   softc - IDE structure
    *      noisy - display stuff as we probe
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

int idecommon_devprobe(idecommon_t *softc,int noisy)
{
    int res;
    int atapi;
    unsigned char buffer[DISK_SECTORSIZE];
    unsigned char model[41];
    uint64_t ttlsect;
    int devtype;
    uint16_t w;
    char *typename;

    /*
     * Reset the drive
     */

    res = idecommon_reset(softc);
    if (res < 0) return -1;

    /*
     * Run diagnostic to get the signature.  
     */

    res = idecommon_diagnostic(softc);
    if (res < 0) return res;

    /*
     * Test signature
     */

    atapi = 0;
    if ((IDE_READREG8(softc,IDE_REG_CYLLSB) == ATAPI_SIG_LSB) &&
	(IDE_READREG8(softc,IDE_REG_CYLMSB) == ATAPI_SIG_MSB)) {
	atapi = 1;
	}

    if (noisy) {
	if (atapi) xprintf("ATAPI: ");
	else xprintf("IDE: ");
	}

    /*
     * Do tha appropriate IDENTIFY command to get device information
     */

    softc->idecommon_atapi = atapi;
    res = idecommon_identify(softc,buffer);
    if (res < 0) return -1;

    /*
     * Using that information, determine our device type
     */

    if (!atapi) {
	devtype = IDE_DEVTYPE_DISK;
	typename = "Disk";
	}
    else {
	w = GETWORD_LE(buffer,0);
	switch ((w >> 8) & 31) {
	    case 5:		/* CD-ROM */
		devtype = IDE_DEVTYPE_CDROM;
		typename = "CD-ROM";
		break;
	    default:
		devtype = IDE_DEVTYPE_ATAPIDISK;
		typename = "Disk";
		break;
	    }
	}

    /*
     * Say nice things about the device.
     */

    idecommon_getmodel(buffer,(char *)model);
    if (noisy) xprintf("%s, \"%s\"",typename,model);
    

#ifdef _IDE_DEBUG_
    if (!softc->idecommon_atapi) {
	ttlsect = (GETWORD_LE(buffer,57) + (GETWORD_LE(buffer,58) << 16));
	if (noisy) xprintf(", Sectors: %llu (%lld MB)",ttlsect,
			   (uint64_t) (ttlsect/2048));
	}
    else {
	ttlsect = 0;
	}
#endif
    if (noisy) xprintf("\n");

    /*
     * Initialize internal structure info, especially pointers to the
     * read/write routines and the sector size.
     */

    softc->idecommon_ttlsect = ttlsect;
    idecommon_init(softc,devtype);

    return res;
}

/*  *********************************************************************
    *  idecommon_open(ctx)
    *  
    *  Process the CFE OPEN call for this device.  For IDE disks,
    *  the device is reset and identified, and the geometry is 
    *  determined.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */


int idecommon_open(cfe_devctx_t *ctx)
{
    idecommon_t *softc = ctx->dev_softc;
    int res;

    if (softc->idecommon_deferprobe) {
	res = idecommon_devprobe(softc,0);
	if (res < 0) return res;
	}

    return 0;
}

/*  *********************************************************************
    *  idecommon_read(ctx,buffer)
    *  
    *  Process a CFE READ command for the IDE device.  This is
    *  more complex than it looks, since CFE offsets are byte offsets
    *  and we may need to read partial sectors.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - buffer descriptor
    *  	   
    *  Return value:
    *  	   number of bytes read, or <0 if an error occured
    ********************************************************************* */

int idecommon_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    idecommon_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;
    int numsec;
    int res = 0;
    int amtcopy;
    uint64_t lba;
    uint64_t offset;
    unsigned char sector[MAX_SECTORSIZE];
    int sectorshift;

    sectorshift = idecommon_sectorshift(softc->idecommon_sectorsize);

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    offset = buffer->buf_offset;
    numsec = (blen + softc->idecommon_sectorsize - 1) >> sectorshift;

    if (offset & (softc->idecommon_sectorsize-1)) {
	lba = (offset >> sectorshift);
	res = (*softc->idecommon_readfunc)(softc,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = softc->idecommon_sectorsize - (offset & (softc->idecommon_sectorsize-1));
	if (amtcopy > blen) amtcopy = blen;
	memcpy(bptr,&sector[offset & (softc->idecommon_sectorsize-1)],amtcopy);
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    while (blen >= softc->idecommon_sectorsize) {
	lba = (offset >> sectorshift);
	amtcopy = softc->idecommon_sectorsize;
	res = (*softc->idecommon_readfunc)(softc,lba,1,bptr);
	if (res < 0) goto out;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    if (blen) {
	lba = (offset >> sectorshift);
	res = (*softc->idecommon_readfunc)(softc,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = blen;
	memcpy(bptr,sector,amtcopy);
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

out:
    buffer->buf_retlen = bptr - buffer->buf_ptr;

    return res;
}

/*  *********************************************************************
    *  idecommon_inpstat(ctx,inpstat)
    *  
    *  Test input status for the IDE disk.  Disks are always ready
    *  to read.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   inpstat - input status structure
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

int idecommon_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    /* idecommon_t *softc = ctx->dev_softc; */

    inpstat->inp_status = 1;
    return 0;
}

/*  *********************************************************************
    *  idecommon_write(ctx,buffer)
    *  
    *  Process a CFE WRITE command for the IDE device.  If the write
    *  involves partial sectors, the affected sectors are read first
    *  and the changes are merged in.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - buffer descriptor
    *  	   
    *  Return value:
    *  	   number of bytes write, or <0 if an error occured
    ********************************************************************* */

int idecommon_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    idecommon_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;
    int numsec;
    int res = 0;
    int amtcopy;
    uint64_t offset;
    uint64_t lba;
    unsigned char sector[MAX_SECTORSIZE];
    int sectorshift;

    sectorshift = (softc->idecommon_sectorsize == 2048) ? 11 : 9;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    offset = buffer->buf_offset;
    numsec = (blen + softc->idecommon_sectorsize - 1) >> sectorshift;

    if (offset & (softc->idecommon_sectorsize-1)) {
	lba = (offset >> sectorshift);
	res = (*softc->idecommon_readfunc)(softc,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = softc->idecommon_sectorsize - (offset & (softc->idecommon_sectorsize-1));
	if (amtcopy > blen) amtcopy = blen;
	memcpy(&sector[offset & (softc->idecommon_sectorsize-1)],bptr,amtcopy);
	res = (*softc->idecommon_writefunc)(softc,lba,1,sector);
	if (res < 0) goto out;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    while (blen >= softc->idecommon_sectorsize) {
	amtcopy = softc->idecommon_sectorsize;
	lba = (offset >> sectorshift);
	res = (*softc->idecommon_writefunc)(softc,lba,1,bptr);
	if (res < 0) goto out;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    if (blen) {
	lba = (offset >> sectorshift);
	res = (*softc->idecommon_readfunc)(softc,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = blen;
	memcpy(sector,bptr,amtcopy);
	res = (*softc->idecommon_writefunc)(softc,lba,1,sector);
	if (res < 0) goto out;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

out:
    buffer->buf_retlen = bptr - buffer->buf_ptr;

    return res;
}


/*  *********************************************************************
    *  idecommon_ioctl(ctx,buffer)
    *  
    *  Process device I/O control requests for the IDE device.
    * 
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - buffer descriptor
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int idecommon_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    idecommon_t *softc = ctx->dev_softc;
    unsigned int *info = (unsigned int *) buffer->buf_ptr;
    unsigned long long *linfo = (unsigned long long *) buffer->buf_ptr;
    blockdev_info_t *devinfo;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_BLOCK_GETBLOCKSIZE:
	    *info = softc->idecommon_sectorsize;
	    break;
	case IOCTL_BLOCK_GETTOTALBLOCKS:
	    *linfo = softc->idecommon_ttlsect;
	    break;
	case IOCTL_BLOCK_GETDEVTYPE:
	    devinfo = (blockdev_info_t *) buffer->buf_ptr;
	    devinfo->blkdev_totalblocks = softc->idecommon_ttlsect;
	    devinfo->blkdev_blocksize = softc->idecommon_sectorsize;
	    devinfo->blkdev_devtype = (softc->idecommon_devtype == IDE_DEVTYPE_CDROM) ?
		BLOCK_DEVTYPE_CDROM : BLOCK_DEVTYPE_DISK;
	    break;
	default:
	    return -1;
	}

    return 0;
}

/*  *********************************************************************
    *  idecommon_close(ctx)
    *  
    *  Close the I/O device.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

int idecommon_close(cfe_devctx_t *ctx)
{
    /* idecommon_t *softc = ctx->dev_softc; */

    return 0;
}


/*  *********************************************************************
    *  idecommon_init(ide,devtype)
    *  
    *  Set up internal values based on the device type
    *  
    *  Input parameters: 
    *  	   ide - IDE interface
    *  	   devtype - device type
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void idecommon_init(idecommon_t *ide,int devtype)
{

    ide->idecommon_devtype = devtype;

    switch (ide->idecommon_devtype) {
	case IDE_DEVTYPE_DISK:
	    ide->idecommon_atapi = FALSE;
	    ide->idecommon_sectorsize = DISK_SECTORSIZE;
	    break;
	case IDE_DEVTYPE_CDROM:
	    ide->idecommon_atapi = TRUE;
	    ide->idecommon_sectorsize = CDROM_SECTORSIZE;
	    break;
	case IDE_DEVTYPE_ATAPIDISK:
	    ide->idecommon_atapi = TRUE;
	    ide->idecommon_sectorsize = DISK_SECTORSIZE;
	    break;
	default:
	    ide->idecommon_atapi = FALSE;
	    ide->idecommon_sectorsize = DISK_SECTORSIZE;
	    break;
	}

    if (ide->idecommon_atapi) {
	ide->idecommon_readfunc = idecommon_read_atapi;
	ide->idecommon_writefunc = idecommon_write_atapi;
	}
    else {
	ide->idecommon_readfunc = idecommon_read_lba;
	ide->idecommon_writefunc = idecommon_write_lba;
	}
}

/*  *********************************************************************
    *  idecommon_attach(devdisp)
    *  
    *  Set up a cfe_devdisp structure that points at the idecommon
    *  structures.
    *  
    *  Input parameters: 
    *  	   devdisp - cfe_devdisp_t structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void idecommon_attach(cfe_devdisp_t *disp)
{
    disp->dev_open = idecommon_open;
    disp->dev_read = idecommon_read;
    disp->dev_inpstat = idecommon_inpstat;
    disp->dev_write = idecommon_write;
    disp->dev_ioctl = idecommon_ioctl;
    disp->dev_close = idecommon_close;
    disp->dev_poll = NULL;
    disp->dev_reset = NULL;
}
