/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB Mass-Storage driver			File: usbmass.c
    *  
    *  This driver deals with mass-storage devices that support
    *  the SCSI Transparent command set and USB Bulk-Only protocol
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
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"
#include "cfe_console.h"
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"

/*  *********************************************************************
    *  USB Mass-Storage class Constants
    ********************************************************************* */

#define USBMASS_CBI_PROTOCOL	0
#define USBMASS_CBI_NOCOMPLETE_PROTOCOL 1
#define USBMASS_BULKONLY_PROTOCOL 0x50

#define USBMASS_SUBCLASS_RBC	0x01
#define USBMASS_SUBCLASS_SFF8020 0x02
#define USBMASS_SUBCLASS_QIC157	0x03
#define USBMASS_SUBCLASS_UFI	0x04
#define USBMASS_SUBCLASS_SFF8070 0x05
#define USBMASS_SUBCLASS_SCSI	0x06

#define USBMASS_CSW_PASS	0x00
#define USBMASS_CSW_FAILED	0x01
#define USBMASS_CSW_PHASEERR	0x02

#define USBMASS_CBW_SIGNATURE	0x43425355
#define USBMASS_CSW_SIGNATURE	0x53425355

/*  *********************************************************************
    *  USB Mass-Storage class Structures
    ********************************************************************* */

typedef struct usbmass_cbw_s {
    uint8_t dCBWSignature0,dCBWSignature1,dCBWSignature2,dCBWSignature3;
    uint8_t dCBWTag0,dCBWTag1,dCBWTag2,dCBWTag3;
    uint8_t dCBWDataTransferLength0,dCBWDataTransferLength1,
	dCBWDataTransferLength2,dCBWDataTransferLength3;
    uint8_t bmCBWFlags;
    uint8_t bCBWLUN;
    uint8_t bCBWCBLength;
    uint8_t CBWCB[16];
} usbmass_cbw_t;

typedef struct usbmass_csw_s {
    uint8_t dCSWSignature0,dCSWSignature1,dCSWSignature2,dCSWSignature3;
    uint8_t dCSWTag0,dCSWTag1,dCSWTag2,dCSWTag3;
    uint8_t dCSWDataResidue0,dCSWDataResidue1,dCSWDataResidue2,dCSWDataResidue3;
    uint8_t bCSWStatus;
} usbmass_csw_t;

#define GETCBWFIELD(s,f) ((uint32_t)((s)->f##0) | ((uint32_t)((s)->f##1) << 8) | \
                          ((uint32_t)((s)->f##2) << 16) | ((uint32_t)((s)->f##3) << 24))
#define PUTCBWFIELD(s,f,v) (s)->f##0 = (v & 0xFF); \
                           (s)->f##1 = ((v)>>8 & 0xFF); \
                           (s)->f##2 = ((v)>>16 & 0xFF); \
                           (s)->f##3 = ((v)>>24 & 0xFF);


int usbmass_request_sense(usbdev_t *dev);

/*  *********************************************************************
    *  Linkage to CFE
    ********************************************************************* */

#ifdef _CFE_

/*
 * Softc for the CFE side of the disk driver.
 */
#define MAX_SECTORSIZE 2048
typedef struct usbdisk_s {
    uint32_t usbdisk_sectorsize;
    uint32_t usbdisk_ttlsect;
    uint32_t usbdisk_devtype;
    int usbdisk_unit;
} usbdisk_t;

/*
 * This table points at the currently configured USB disk 
 * devices.  This lets us leave the CFE half of the driver lying
 * around while the USB devices come and go.  We use the unit number
 * from the original CFE attach to index this table, and devices
 * that are not present are "not ready."
 */

#define USBDISK_MAXUNITS	4
static usbdev_t *usbdisk_units[USBDISK_MAXUNITS];

/*
 * CFE device driver routine forwards
 */

static void usbdisk_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr);

static int usbdisk_open(cfe_devctx_t *ctx);
static int usbdisk_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int usbdisk_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int usbdisk_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int usbdisk_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int usbdisk_close(cfe_devctx_t *ctx);

/*
 * CFE device driver descriptor
 */

const static cfe_devdisp_t usbdisk_dispatch = {
    usbdisk_open,
    usbdisk_read,
    usbdisk_inpstat,
    usbdisk_write,
    usbdisk_ioctl,
    usbdisk_close,	
    NULL,
    NULL
};

const cfe_driver_t usb_disk = {
    "USB Disk",
    "usbdisk",
    CFE_DEV_DISK,
    &usbdisk_dispatch,
    usbdisk_probe
};


#endif



/*  *********************************************************************
    *  Forward Definitions
    ********************************************************************* */

static int usbmass_attach(usbdev_t *dev,usb_driver_t *drv);
static int usbmass_detach(usbdev_t *dev);

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct usbmass_softc_s {
    int umass_inpipe;
    int umass_outpipe;
    int umass_devtype;
    uint32_t umass_curtag;
    int umass_unit;
} usbmass_softc_t;

usb_driver_t usbmass_driver = {
    "Mass-Storage Device",
    usbmass_attach,
    usbmass_detach
};

usbdev_t *usbmass_dev = NULL;		/* XX hack for testing only */

/*  *********************************************************************
    *  usbmass_mass_storage_reset(dev,ifc)
    *  
    *  Do a bulk-only mass-storage reset.
    *  
    *  Input parameters: 
    *  	   dev - device to reset
    *      ifc - interface number to reset (bInterfaceNum)
    *  	   
    *  Return value:
    *  	   status
    ********************************************************************* */

#define usbmass_mass_storage_reset(dev,ifc) \
     usb_simple_request(dev,0x21,0xFF,ifc,0)



/*  *********************************************************************
    *  usbmass_stall_recovery(dev)
    *  
    *  Do whatever it takes to unstick a stalled mass-storage device.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usbmass_stall_recovery(usbdev_t *dev)
{
    usbmass_softc_t *softc;

    softc = (usbmass_softc_t *) dev->ud_private;

    usb_clear_stall(dev,softc->umass_inpipe);

    usbmass_request_sense(dev);
}


/*  *********************************************************************
    *  usbmass_read_capacity(dev,sectornum,buffer)
    *  
    *  Reads a sector from the device.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   sectornum - sector number to read
    *  	   buffer - place to put sector we read
    *  	   
    *  Return value:
    *  	   status
    ********************************************************************* */

int usbmass_request_sense(usbdev_t *dev)
{
    uint8_t *cbwcsw;
    uint8_t *sector;
    usbmass_cbw_t *cbw;
    usbmass_csw_t *csw;
    usbreq_t *ur;
    usbmass_softc_t *softc;
    int res;

    softc = (usbmass_softc_t *) dev->ud_private;

    cbwcsw = KMALLOC(64,32);
    sector = KMALLOC(64,32);

    memset(sector,0,64);

    cbw = (usbmass_cbw_t *) cbwcsw;
    csw = (usbmass_csw_t *) cbwcsw;

    /*
     * Fill in the fields of the CBW
     */

    PUTCBWFIELD(cbw,dCBWSignature,USBMASS_CBW_SIGNATURE);
    PUTCBWFIELD(cbw,dCBWTag,softc->umass_curtag);
    PUTCBWFIELD(cbw,dCBWDataTransferLength,18);
    cbw->bmCBWFlags = 0x80;		/* IN */
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 12;
    cbw->CBWCB[0] = 0x3;		/* REQUEST SENSE */
    cbw->CBWCB[1] = 0;
    cbw->CBWCB[2] = 0;
    cbw->CBWCB[3] = 0;
    cbw->CBWCB[4] = 18;			/* allocation length */
    cbw->CBWCB[5] = 0;
    cbw->CBWCB[6] = 0;
    cbw->CBWCB[7] = 0;
    cbw->CBWCB[8] = 0;
    cbw->CBWCB[9] = 0;

    softc->umass_curtag++;

    /*
     * Send the CBW 
     */

    ur = usb_make_request(dev,softc->umass_outpipe,(uint8_t *) cbw,
			  sizeof(usbmass_cbw_t),UR_FLAG_OUT);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    /*
     * Get the data
     */

    memset(sector,0,18);
    ur = usb_make_request(dev,softc->umass_inpipe,sector,
			  18,UR_FLAG_IN | UR_FLAG_SHORTOK);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    /*
     * Get the Status
     */

    memset(csw,0,sizeof(usbmass_csw_t));
    ur = usb_make_request(dev,softc->umass_inpipe,(uint8_t *) csw,
			  sizeof(usbmass_csw_t),UR_FLAG_IN);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    KFREE(cbwcsw);

    KFREE(sector);

    return 0;

}

/*  *********************************************************************
    *  usbmass_read_sector(dev,sectornum,seccnt,buffer)
    *  
    *  Reads a sector from the device.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   sectornum - sector number to read
    * 	   seccnt - count of sectors to read
    *  	   buffer - place to put sector we read
    *  	   
    *  Return value:
    *  	   status
    ********************************************************************* */

int usbmass_read_sector(usbdev_t *dev,uint32_t sectornum,uint32_t seccnt,
			uint8_t *buffer);
int usbmass_read_sector(usbdev_t *dev,uint32_t sectornum,uint32_t seccnt,
				uint8_t *buffer)
{
    uint8_t *cbwcsw;
    uint8_t *sector;
    usbmass_cbw_t *cbw;
    usbmass_csw_t *csw;
    usbreq_t *ur;
    usbmass_softc_t *softc;
    int res;

    softc = (usbmass_softc_t *) dev->ud_private;

    cbwcsw = KMALLOC(64,32);
    sector = buffer;

    cbw = (usbmass_cbw_t *) cbwcsw;
    csw = (usbmass_csw_t *) cbwcsw;

    /*
     * Fill in the fields of the CBW
     */

    PUTCBWFIELD(cbw,dCBWSignature,USBMASS_CBW_SIGNATURE);
    PUTCBWFIELD(cbw,dCBWTag,softc->umass_curtag);
    PUTCBWFIELD(cbw,dCBWDataTransferLength,(512*seccnt));
    cbw->bmCBWFlags = 0x80;		/* IN */
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 10;
    cbw->CBWCB[0] = 0x28;		/* READ */
    cbw->CBWCB[1] = 0;
    cbw->CBWCB[2] = (sectornum >> 24) & 0xFF;	/* LUN 0 & MSB's of sector */
    cbw->CBWCB[3] = (sectornum >> 16) & 0xFF;
    cbw->CBWCB[4] = (sectornum >>  8) & 0xFF;
    cbw->CBWCB[5] = (sectornum >>  0) & 0xFF;
    cbw->CBWCB[6] = 0;
    cbw->CBWCB[7] = 0;
    cbw->CBWCB[8] = seccnt;
    cbw->CBWCB[9] = 0;

    softc->umass_curtag++;

    /*
     * Send the CBW 
     */

    ur = usb_make_request(dev,softc->umass_outpipe,(uint8_t *) cbw,
			  sizeof(usbmass_cbw_t),UR_FLAG_OUT);
    res = usb_sync_request(ur);
    usb_free_request(ur);
    if (res == 4) {
	usbmass_stall_recovery(dev);
	KFREE(cbwcsw);
	return -1;
	}


    /*
     * Get the data
     */

    ur = usb_make_request(dev,softc->umass_inpipe,sector,
			  512*seccnt,UR_FLAG_IN | UR_FLAG_SHORTOK);
    res = usb_sync_request(ur);
    usb_free_request(ur);
    if (res == 4) {
	usbmass_stall_recovery(dev);
	KFREE(cbwcsw);
	return -1;
	}


    /*
     * Get the Status
     */

    memset(csw,0,sizeof(usbmass_csw_t));
    ur = usb_make_request(dev,softc->umass_inpipe,(uint8_t *) csw,
			  sizeof(usbmass_csw_t),UR_FLAG_IN);
    res = usb_sync_request(ur);
    usb_free_request(ur);
    if (res == 4) {
	usbmass_stall_recovery(dev);
	KFREE(cbwcsw);
	return -1;
	}



    res = (csw->bCSWStatus == USBMASS_CSW_PASS) ? 0 : -1;

    KFREE(cbwcsw);

    return res;

}

/*  *********************************************************************
    *  usbmass_write_sector(dev,sectornum,seccnt,buffer)
    *  
    *  Writes a sector to the device
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   sectornum - sector number to write
    * 	   seccnt - count of sectors to write
    *  	   buffer - place to get sector to write
    *  	   
    *  Return value:
    *  	   status
    ********************************************************************* */

static int usbmass_write_sector(usbdev_t *dev,uint32_t sectornum,uint32_t seccnt,
				uint8_t *buffer)
{
    uint8_t *cbwcsw;
    uint8_t *sector;
    usbmass_cbw_t *cbw;
    usbmass_csw_t *csw;
    usbreq_t *ur;
    usbmass_softc_t *softc;
    int res;

    softc = (usbmass_softc_t *) dev->ud_private;

    cbwcsw = KMALLOC(64,32);
    sector = buffer;

    cbw = (usbmass_cbw_t *) cbwcsw;
    csw = (usbmass_csw_t *) cbwcsw;

    /*
     * Fill in the fields of the CBW
     */

    PUTCBWFIELD(cbw,dCBWSignature,USBMASS_CBW_SIGNATURE);
    PUTCBWFIELD(cbw,dCBWTag,softc->umass_curtag);
    PUTCBWFIELD(cbw,dCBWDataTransferLength,(512*seccnt));
    cbw->bmCBWFlags = 0x00;		/* OUT */
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 10;
    cbw->CBWCB[0] = 0x2A;		/* WRITE */
    cbw->CBWCB[1] = 0;
    cbw->CBWCB[2] = (sectornum >> 24) & 0xFF;	/* LUN 0 & MSB's of sector */
    cbw->CBWCB[3] = (sectornum >> 16) & 0xFF;
    cbw->CBWCB[4] = (sectornum >>  8) & 0xFF;
    cbw->CBWCB[5] = (sectornum >>  0) & 0xFF;
    cbw->CBWCB[6] = 0;
    cbw->CBWCB[7] = 0;
    cbw->CBWCB[8] = seccnt;
    cbw->CBWCB[9] = 0;

    softc->umass_curtag++;

    /*
     * Send the CBW 
     */

    ur = usb_make_request(dev,softc->umass_outpipe,(uint8_t *) cbw,
			  sizeof(usbmass_cbw_t),UR_FLAG_OUT);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    /*
     * Send the data
     */

    ur = usb_make_request(dev,softc->umass_outpipe,sector,
			  512*seccnt,UR_FLAG_OUT);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    /*
     * Get the Status
     */

    memset(csw,0,sizeof(usbmass_csw_t));
    ur = usb_make_request(dev,softc->umass_inpipe,(uint8_t *) csw,
			  sizeof(usbmass_csw_t),UR_FLAG_IN);
    res = usb_sync_request(ur);
    usb_free_request(ur);


    res = (csw->bCSWStatus == USBMASS_CSW_PASS) ? 0 : -1;

    KFREE(cbwcsw);

    return res;
}

/*  *********************************************************************
    *  usbmass_read_capacity(dev,sectornum,buffer)
    *  
    *  Reads a sector from the device.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   sectornum - sector number to read
    *  	   buffer - place to put sector we read
    *  	   
    *  Return value:
    *  	   status
    ********************************************************************* */

int usbmass_read_capacity(usbdev_t *dev,uint32_t *size);
int usbmass_read_capacity(usbdev_t *dev,uint32_t *size)
{
    uint8_t *cbwcsw;
    uint8_t *sector;
    usbmass_cbw_t *cbw;
    usbmass_csw_t *csw;
    usbreq_t *ur;
    usbmass_softc_t *softc;
    int res;

    softc = (usbmass_softc_t *) dev->ud_private;

    cbwcsw = KMALLOC(64,32);
    sector = KMALLOC(64,32);

    memset(sector,0,64);

    cbw = (usbmass_cbw_t *) cbwcsw;
    csw = (usbmass_csw_t *) cbwcsw;

    *size = 0;

    /*
     * Fill in the fields of the CBW
     */

    PUTCBWFIELD(cbw,dCBWSignature,USBMASS_CBW_SIGNATURE);
    PUTCBWFIELD(cbw,dCBWTag,softc->umass_curtag);
    PUTCBWFIELD(cbw,dCBWDataTransferLength,8);
    cbw->bmCBWFlags = 0x80;		/* IN */
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 10;
    cbw->CBWCB[0] = 0x25;		/* READ CAPACITY */
    cbw->CBWCB[1] = 0;
    cbw->CBWCB[2] = 0;
    cbw->CBWCB[3] = 0;
    cbw->CBWCB[4] = 0;
    cbw->CBWCB[5] = 0;
    cbw->CBWCB[6] = 0;
    cbw->CBWCB[7] = 0;
    cbw->CBWCB[8] = 0;
    cbw->CBWCB[9] = 0;

    softc->umass_curtag++;

    /*
     * Send the CBW 
     */

    ur = usb_make_request(dev,softc->umass_outpipe,(uint8_t *) cbw,
			  sizeof(usbmass_cbw_t),UR_FLAG_OUT);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    if (res == 4) {
	usbmass_stall_recovery(dev);
	KFREE(cbwcsw);
	KFREE(sector);
	return -1;
	}

    /*
     * Get the data
     */

    ur = usb_make_request(dev,softc->umass_inpipe,sector,
			  8,UR_FLAG_IN | UR_FLAG_SHORTOK);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    if (res == 4) {
	usbmass_stall_recovery(dev);
	KFREE(cbwcsw);
	KFREE(sector);
	return -1;
	}

    /*
     * Get the Status
     */

    memset(csw,0,sizeof(usbmass_csw_t));
    ur = usb_make_request(dev,softc->umass_inpipe,(uint8_t *) csw,
			  sizeof(usbmass_csw_t),UR_FLAG_IN);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    KFREE(cbwcsw);

    *size = (((uint32_t) sector[0]) << 24) |
	(((uint32_t) sector[1]) << 16) |
	(((uint32_t) sector[2]) << 8) |
	(((uint32_t) sector[3]) << 0);

    KFREE(sector);

    return 0;

}



/*  *********************************************************************
    *  usbmass_attach(dev,drv)
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

static int usbmass_attach(usbdev_t *dev,usb_driver_t *drv)
{
    usb_config_descr_t *cfgdscr = dev->ud_cfgdescr;
    usb_endpoint_descr_t *epdscr;
    usb_endpoint_descr_t *indscr = NULL;
    usb_endpoint_descr_t *outdscr = NULL;
    usb_interface_descr_t *ifdscr;
    usbmass_softc_t *softc;
    int idx;

    dev->ud_drv = drv;

    softc = KMALLOC(sizeof(usbmass_softc_t),0);
    memset(softc,0,sizeof(usbmass_softc_t));
    dev->ud_private = softc;

    ifdscr = usb_find_cfg_descr(dev,USB_INTERFACE_DESCRIPTOR_TYPE,0);
    if (ifdscr == NULL) {
	return -1;
	}

    if ((ifdscr->bInterfaceSubClass != USBMASS_SUBCLASS_SCSI) ||
	(ifdscr->bInterfaceProtocol != USBMASS_BULKONLY_PROTOCOL)) {
	console_log("USBMASS: Do not understand devices with SubClass 0x%02X, Protocol 0x%02X",
		    ifdscr->bInterfaceSubClass,
		    ifdscr->bInterfaceProtocol);
	return -1;
	}

    for (idx = 0; idx < 2; idx++) {
	epdscr = usb_find_cfg_descr(dev,USB_ENDPOINT_DESCRIPTOR_TYPE,idx);

	if (USB_ENDPOINT_DIR_OUT(epdscr->bEndpointAddress)) {
	    outdscr = epdscr;
	    }
	else {
	    indscr = epdscr;
	    }
	}


    if (!indscr || !outdscr) {
	/*
	 * Could not get descriptors, something is very wrong.
	 * Leave device addressed but not configured.
	 */
	return -1;
	}

    /*
     * Choose the standard configuration.
     */

    usb_set_configuration(dev,cfgdscr->bConfigurationValue);

    /*
     * Open the pipes.
     */

    softc->umass_inpipe     = usb_open_pipe(dev,indscr);
    softc->umass_outpipe    = usb_open_pipe(dev,outdscr);
    softc->umass_curtag     = 0x12345678;

    /*
     * Save pointer in global unit table so we can
     * match CFE devices up with USB ones
     */


#ifdef _CFE_
    softc->umass_unit = -1;
    for (idx = 0; idx < USBDISK_MAXUNITS; idx++) {
	if (usbdisk_units[idx] == NULL) {
	    softc->umass_unit = idx;
	    usbdisk_units[idx] = dev;
	    break;
	    }
	}

    console_log("USBMASS: Unit %d connected",softc->umass_unit);
#endif

    usbmass_dev = dev;

    return 0;
}

/*  *********************************************************************
    *  usbmass_detach(dev)
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

static int usbmass_detach(usbdev_t *dev)
{
    usbmass_softc_t *softc;
    softc = (usbmass_softc_t *) dev->ud_private;

#ifdef _CFE_
    console_log("USBMASS: USB unit %d disconnected",softc->umass_unit);
    if (softc->umass_unit >= 0) usbdisk_units[softc->umass_unit] = NULL;
#endif

    KFREE(softc);
    return 0;
}
 


#ifdef _CFE_


/*  *********************************************************************
    *  usbdisk_sectorshift(size)
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

#define usbdisk_sectorshift(size) (((size)==2048)?11:9)


/*  *********************************************************************
    *  usbdisk_probe(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Our probe routine.  Attach an empty USB disk device to the firmware.
    *  
    *  Input parameters: 
    *  	   drv - driver structure
    *  	   probe_a - not used
    *  	   probe_b - not used
    *  	   probe_ptr - not used
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usbdisk_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    usbdisk_t *softc;
    char descr[128];

    softc = (usbdisk_t *) KMALLOC(sizeof(usbdisk_t),0);

    memset(softc,0,sizeof(usbdisk_t));

    softc->usbdisk_sectorsize = 512;
    softc->usbdisk_devtype = BLOCK_DEVTYPE_DISK;
    softc->usbdisk_ttlsect = 0;		/* not calculated yet */
    softc->usbdisk_unit = (int)probe_a;

    xsprintf(descr,"USB Disk unit %d",(int)probe_a);

    cfe_attach(drv,softc,NULL,descr);
}


/*  *********************************************************************
    *  usbdisk_open(ctx)
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


static int usbdisk_open(cfe_devctx_t *ctx)
{
    usbdisk_t *softc = ctx->dev_softc;
    usbdev_t *dev = usbdisk_units[softc->usbdisk_unit];
    uint32_t size;
    int res;

    if (!dev) return CFE_ERR_NOTREADY;

    usbmass_request_sense(dev);

    res = usbmass_read_capacity(dev,&size);
    if (res < 0) return res;

    softc->usbdisk_ttlsect = size;

    return 0;
}

/*  *********************************************************************
    *  usbdisk_read(ctx,buffer)
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

static int usbdisk_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    usbdisk_t *softc = ctx->dev_softc;
    usbdev_t *dev = usbdisk_units[softc->usbdisk_unit];
    unsigned char *bptr;
    int blen;
    int numsec;
    int res = 0;
    int amtcopy;
    uint64_t lba;
    uint64_t offset;
    unsigned char sector[MAX_SECTORSIZE];
    int sectorshift;

    if (!dev) return CFE_ERR_NOTREADY;

    sectorshift = usbdisk_sectorshift(softc->usbdisk_sectorsize);

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    offset = buffer->buf_offset;
    numsec = (blen + softc->usbdisk_sectorsize - 1) >> sectorshift;

    if (offset & (softc->usbdisk_sectorsize-1)) {
	lba = (offset >> sectorshift);
	res = usbmass_read_sector(dev,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = softc->usbdisk_sectorsize - (offset & (softc->usbdisk_sectorsize-1));
	if (amtcopy > blen) amtcopy = blen;
	memcpy(bptr,&sector[offset & (softc->usbdisk_sectorsize-1)],amtcopy);
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    if (blen >= softc->usbdisk_sectorsize) {
	int seccnt;

	lba = (offset >> sectorshift);
	seccnt = (blen >> sectorshift);

	res = usbmass_read_sector(dev,lba,seccnt,bptr);
	if (res < 0) goto out;

	amtcopy = seccnt << sectorshift;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    if (blen) {
	lba = (offset >> sectorshift);
	res = usbmass_read_sector(dev,lba,1,sector);
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
    *  usbdisk_inpstat(ctx,inpstat)
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

static int usbdisk_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    /* usbdisk_t *softc = ctx->dev_softc; */

    inpstat->inp_status = 1;
    return 0;
}

/*  *********************************************************************
    *  usbdisk_write(ctx,buffer)
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

static int usbdisk_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    usbdisk_t *softc = ctx->dev_softc;
    usbdev_t *dev = usbdisk_units[softc->usbdisk_unit];
    unsigned char *bptr;
    int blen;
    int numsec;
    int res = 0;
    int amtcopy;
    uint64_t offset;
    uint64_t lba;
    unsigned char sector[MAX_SECTORSIZE];
    int sectorshift;

    if (!dev) return CFE_ERR_NOTREADY;

    sectorshift = usbdisk_sectorshift(softc->usbdisk_sectorsize);

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    offset = buffer->buf_offset;
    numsec = (blen + softc->usbdisk_sectorsize - 1) >> sectorshift;

    if (offset & (softc->usbdisk_sectorsize-1)) {
	lba = (offset >> sectorshift);
	res = usbmass_read_sector(dev,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = softc->usbdisk_sectorsize - (offset & (softc->usbdisk_sectorsize-1));
	if (amtcopy > blen) amtcopy = blen;
	memcpy(&sector[offset & (softc->usbdisk_sectorsize-1)],bptr,amtcopy);
	res = usbmass_write_sector(dev,lba,1,sector);
	if (res < 0) goto out;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    while (blen >= softc->usbdisk_sectorsize) {
	amtcopy = softc->usbdisk_sectorsize;
	lba = (offset >> sectorshift);
	res = usbmass_write_sector(dev,lba,1,bptr);
	if (res < 0) goto out;
	bptr += amtcopy;
	offset += amtcopy;
	blen -= amtcopy;
	}

    if (blen) {
	lba = (offset >> sectorshift);
	res = usbmass_read_sector(dev,lba,1,sector);
	if (res < 0) goto out;
	amtcopy = blen;
	memcpy(sector,bptr,amtcopy);
	res = usbmass_write_sector(dev,lba,1,sector);
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
    *  usbdisk_ioctl(ctx,buffer)
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

static int usbdisk_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    usbdisk_t *softc = ctx->dev_softc;
    unsigned int *info = (unsigned int *) buffer->buf_ptr;
    unsigned long long *linfo = (unsigned long long *) buffer->buf_ptr;
    blockdev_info_t *devinfo;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_BLOCK_GETBLOCKSIZE:
	    *info = softc->usbdisk_sectorsize;
	    break;
	case IOCTL_BLOCK_GETTOTALBLOCKS:
	    *linfo = softc->usbdisk_ttlsect;
	    break;
	case IOCTL_BLOCK_GETDEVTYPE:
	    devinfo = (blockdev_info_t *) buffer->buf_ptr;
	    devinfo->blkdev_totalblocks = softc->usbdisk_ttlsect;
	    devinfo->blkdev_blocksize = softc->usbdisk_sectorsize;
	    devinfo->blkdev_devtype = softc->usbdisk_devtype;
	    break;
	default:
	    return -1;
	}

    return 0;
}

/*  *********************************************************************
    *  usbdisk_close(ctx)
    *  
    *  Close the I/O device.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int usbdisk_close(cfe_devctx_t *ctx)
{
    /* usbdisk_t *softc = ctx->dev_softc; */

    return 0;
}


#endif
