/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Null console device			File: dev_null.c
    *  
    *  This is a null console device, useful for console-less
    *  operation, or when using chip simulators.
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
#include "cfe_iocb.h"
#include "cfe_device.h"

static void nulldrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int nulldrv_open(cfe_devctx_t *ctx);
static int nulldrv_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int nulldrv_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int nulldrv_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int nulldrv_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int nulldrv_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t nulldrv_dispatch = {
    nulldrv_open,
    nulldrv_read,
    nulldrv_inpstat,
    nulldrv_write,
    nulldrv_ioctl,
    nulldrv_close,	
    NULL,
    NULL
};

const cfe_driver_t nulldrv = {
    "Null console device",
    "null",
    CFE_DEV_SERIAL,
    &nulldrv_dispatch,
    nulldrv_probe
};


static void nulldrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    cfe_attach(drv,NULL,NULL,drv->drv_description);
}


static int nulldrv_open(cfe_devctx_t *ctx)
{
/*    nulldrv_t *softc = ctx->dev_softc; */

    return 0;
}

static int nulldrv_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
/*    nulldrv_t *softc = ctx->dev_softc; */

    buffer->buf_retlen = buffer->buf_length;
    return 0;
}

static int nulldrv_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
/*    nulldrv_t *softc = ctx->dev_softc; */

    inpstat->inp_status = 0;

    return 0;
}

static int nulldrv_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
/*    nulldrv_t *softc = ctx->dev_softc; */

    buffer->buf_retlen = buffer->buf_length;
    return 0;
}

static int nulldrv_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
/*    nulldrv_t *softc = ctx->dev_softc;*/

    return -1;
}

static int nulldrv_close(cfe_devctx_t *ctx)
{
/*    nulldrv_t *softc = ctx->dev_softc; */

    return 0;
}
