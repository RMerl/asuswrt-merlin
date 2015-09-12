/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCB dispatcher		    	File: cfe_vendor_iocb_dispatch.c
    *  
    *  This routine is the main API dispatch for CFE.  User API
    *  calls, via the ROM entry point, get dispatched to routines
    *  in this module.
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
#include "cfe_vendor_iocb.h"
#include "cfe_error.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#include "cfe_mem.h"
#include "cfe_fileops.h"
#include "cfe_boot.h"
#include "env_subr.h"
#include "cfe.h"
#include "cfe_console.h"
#include "bsp_config.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define HV	1			/* handle valid */

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

extern cfe_devctx_t *cfe_handle_table[CFE_MAX_HANDLE];

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int cfe_vendor_iocb_dispatch(cfe_iocb_t *iocb);

/*  *********************************************************************
    *  Dispatch table
    ********************************************************************* */

struct cfe_vendor_cmd_dispatch_s {
    int plistsize;
    int flags;
    int (*func)(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
};


static int cfe_cmd_vendor_sample(cfe_devctx_t *ctx,cfe_iocb_t *iocb);

const static struct cfe_vendor_cmd_dispatch_s 
   cfe_vendor_cmd_dispatch_table[CFE_CMD_VENDOR_MAX - CFE_CMD_VENDOR_USE] = {
    {sizeof(iocb_buffer_t), 0,	cfe_cmd_vendor_sample},		/* 0 : CFE_CMD_VENDOR_SAMPLE */
};

/*  *********************************************************************
    *  IOCB dispatch routines
    ********************************************************************* */


int cfe_vendor_iocb_dispatch(cfe_iocb_t *iocb)
{
    const struct cfe_vendor_cmd_dispatch_s *disp;
    int res;
    cfe_devctx_t *ctx;

    /*
     * Check for commands codes out of range
     */

    if ((iocb->iocb_fcode < CFE_CMD_VENDOR_USE) || 
	(iocb->iocb_fcode >= CFE_CMD_VENDOR_MAX)) {
	iocb->iocb_status = CFE_ERR_INV_COMMAND;
	return iocb->iocb_status;
	}

    /*
     * Check for command codes in range but invalid
     */

    disp = &cfe_vendor_cmd_dispatch_table[iocb->iocb_fcode - CFE_CMD_VENDOR_USE];

    if (disp->plistsize < 0) {
	iocb->iocb_status = CFE_ERR_INV_COMMAND;
	return iocb->iocb_status;
	}

    /*
     * Check for invalid parameter list size
     */

    if (disp->plistsize != iocb->iocb_psize) {
	iocb->iocb_status = CFE_ERR_INV_PARAM;
	return iocb->iocb_status;
	}

    /*
     * Determine handle
     */
    
    ctx = NULL;
    if (disp->flags & HV) {
	if ((iocb->iocb_handle >= CFE_MAX_HANDLE) || 
	    (iocb->iocb_handle < 0) ||
	    (cfe_handle_table[iocb->iocb_handle] == NULL)){
	    iocb->iocb_status = CFE_ERR_INV_PARAM;
	    return iocb->iocb_status;
	    }
	ctx = cfe_handle_table[iocb->iocb_handle];
	}

    /*
     * Dispatch to handler routine
     */

    res = (*disp->func)(ctx,iocb);

    iocb->iocb_status = res;
    return res;
}



/*  *********************************************************************
    *  Implementation routines for each IOCB function
    ********************************************************************* */

static int cfe_cmd_vendor_sample(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    return CFE_OK;
}
