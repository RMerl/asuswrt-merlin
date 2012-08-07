/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Device Attach routines			File: cfe_attach.c
    *  
    *  This module manages the list of device drivers.  When a driver
    *  is probed, it can call cfe_attach to create actual device
    *  instances.  The routines in this module manage the
    *  device list and the assignment of device names.
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
#include "lib_queue.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_iocb.h"
#include "cfe_device.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define CFE_MAX_DEVINST 64		/* max # of instances of devices */

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

/*
 * Our device list.
 */

queue_t cfe_devices = {&cfe_devices, &cfe_devices};

/*  *********************************************************************
    *  cfe_finddev(name)
    *  
    *  Locate a device in the device list by its name and return
    *  a pointer to the device structure.
    *  
    *  Input parameters: 
    *  	   name - name of device, e.g., "uart0"
    *  	   
    *  Return value:
    *  	   cfe_device_t pointer or NULL
    ********************************************************************* */

cfe_device_t *cfe_finddev(char *name)
{
    queue_t *qb;
    cfe_device_t *dev;

    for (qb = cfe_devices.q_next; qb != &cfe_devices; qb = qb->q_next) {
	dev = (cfe_device_t *) qb;
	if (strcmp(dev->dev_fullname,name) == 0) {
	    return dev;
	    }
	}

    return NULL;
}


/*  *********************************************************************
    *  cfe_device_reset()
    *  
    *  Call all the "reset" methods in the devices on the device 
    *  list.  Note that the methods get called even when the
    *  devices are closed!
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_device_reset(void)
{
    queue_t *qb;
    cfe_device_t *dev;

    for (qb = cfe_devices.q_next; qb != &cfe_devices; qb = qb->q_next) {
	dev = (cfe_device_t *) qb;
	if (dev->dev_dispatch->dev_reset) {
	    (*(dev->dev_dispatch->dev_reset))(dev->dev_softc);
	    }
	}
}

/*  *********************************************************************
    *  cfe_attach_idx(drv,idx,softc,bootinfo,description)
    *  
    *  Add a device to the device list at a specific index.  This
    *  is mainly used for devices like SCSI disks or CD-ROMs so
    *  we can use an index that matches the target ID or LUN.
    *  
    *  Input parameters: 
    *  	   drv - driver structure (from the device driver module)
    *  	   idx - requested index (e.g., uartn where 'n' is the idx)
    *  	   softc - Unique information maintained for this device
    *  	   bootinfo - suffix for long form of the device name.  For
    *  	              example, scsi0.3.1  might mean SCSI controller
    *  	              0, device ID 3, LUN 1.  The bootinfo would be
    *  	              "3.1"
    *  	   description - something nice to say for the devices command
    *  	   
    *  Return value:
    *  	   0 if device has already been added at this index
    *     <0 for other problems
    *  	   1 if we were successful.
    ********************************************************************* */

int cfe_attach_idx(cfe_driver_t *drv,int idx,void *softc,
		   char *bootinfo,char *description)
{
    char name[64];
    cfe_device_t *dev;

    xsprintf(name,"%s%d",drv->drv_bootname,idx);

    if (bootinfo) {
	strcat(name,".");
	strcat(name,bootinfo);
	}

    if (cfe_finddev(name) != NULL) {
	return 0;
	}

    dev = (cfe_device_t *) KMALLOC(sizeof(cfe_device_t),0);
    if (!dev) return -1;

    dev->dev_fullname = strdup(name);
    dev->dev_softc = softc;
    dev->dev_class = drv->drv_class;
    dev->dev_dispatch = drv->drv_dispatch;
    dev->dev_description = description ? strdup(description) : NULL;
    dev->dev_opencount = 0;

    q_enqueue(&cfe_devices,(queue_t *) dev);

    return 1;

}



/*  *********************************************************************
    *  cfe_attach(drv,softc,bootinfo,description
    *  
    *  Add a device to the system.  This is a callback from the
    *  probe routine, and is used to actually add devices to CFE's
    *  device list.
    *  
    *  Input parameters: 
    *  	   drv - driver structure (from the device driver module)
    *  	   softc - Unique information maintained for this device
    *  	   bootinfo - suffix for long form of the device name.  For
    *  	              example, scsi0.3.1  might mean SCSI controller
    *  	              0, device ID 3, LUN 1.  The bootinfo would be
    *  	              "3.1"
    *  	   description - something nice to say for the devices command
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_attach(cfe_driver_t *drv,void *softc,
		char *bootinfo,
		char *description)
{
    int idx;
    int res;

    /*
     * Try device indicies 0..CFE_MAX_DEVINST to assign a unique
     * device name for this device.  This is a really braindead way to 
     * do this, but how many devices are we expecting anyway?
     */

    for (idx = 0; idx < CFE_MAX_DEVINST; idx++) {

	res = cfe_attach_idx(drv,idx,softc,bootinfo,description);

	if (res < 0) break;	/* out of memory or other badness */
	if (res > 0) break;	/* success! */
	/* otherwise, try again, slot is taken */
	}

}

/*  *********************************************************************
    *  cfe_attach_init()
    *  
    *  Initialize this module.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	  
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void cfe_attach_init(void)
{
    q_init(&(cfe_devices));
}


/*  *********************************************************************
    *  cfe_device_name(ctx)
    *  
    *  Given a device context, return a device name
    *  
    *  Input parameters: 
    *  	   ctx - context
    *  	   
    *  Return value:
    *  	   name
    ********************************************************************* */

char *cfe_device_name(cfe_devctx_t *ctx)
{
    return ctx->dev_dev->dev_fullname;
}
