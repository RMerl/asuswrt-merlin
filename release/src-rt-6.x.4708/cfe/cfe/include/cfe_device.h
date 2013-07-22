/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Device manager definitions		File: cfe_device.h
    *  
    *  Structures, constants, etc. for the device manager, which keeps
    *  track of installed devices in CFE.
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


#ifndef _CFE_DEVICE_H
#define _CFE_DEVICE_H

#include "lib_queue.h"

typedef struct cfe_devdisp_s cfe_devdisp_t;


/*
 * The Device structure defines a particular instance of a device.  
 * They are generated as a result of calling the cfe_attach call.
 */

typedef struct cfe_device_s {
    queue_t dev_next;
    char *dev_fullname;
    void *dev_softc;
    int dev_class;
    const cfe_devdisp_t *dev_dispatch;
    int dev_opencount;
    char *dev_description;
} cfe_device_t;

/*
 * This is what gets returned from the OPEN call
 */
typedef struct cfe_devctx_s {		
    cfe_device_t *dev_dev;
    void *dev_softc;
    void *dev_openinfo;
} cfe_devctx_t;


/*
 * This defines a given device class.  Even though there are
 * three identical MACs, there is only one of these.
 */

struct cfe_devdisp_s {
    int (*dev_open)(cfe_devctx_t *ctx);
    int (*dev_read)(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
    int (*dev_inpstat)(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
    int (*dev_write)(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
    int (*dev_ioctl)(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
    int (*dev_close)(cfe_devctx_t *ctx);
    void (*dev_poll)(cfe_devctx_t *ctx,int64_t ticks);
    void (*dev_reset)(void *softc);	/* called when device is closed, so no devctx_t */
};



typedef struct cfe_driver_s {
    char *drv_description;		/* Description of device for SHOW commands */
    char *drv_bootname;			/* Device's name prefix for open() */
    int drv_class;
    const cfe_devdisp_t *drv_dispatch;
    void (*drv_probe)(struct cfe_driver_s *drv,
		      unsigned long probe_a, unsigned long probe_b, 
		      void *probe_ptr);
} cfe_driver_t;

char *cfe_device_name(cfe_devctx_t *ctx);
void cfe_attach(cfe_driver_t *devname,void *softc,char *bootinfo,char *description);
int cfe_attach_idx(cfe_driver_t *drv,int idx,void *softc,char *bootinfo,char *description);
cfe_device_t *cfe_finddev(char *name);
void cfe_attach_init(void);
#define cfe_add_device(devdescr,a,b,ptr) (devdescr)->drv_probe(devdescr,a,b,ptr)    
void cfe_device_reset(void);

#endif
