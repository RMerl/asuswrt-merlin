/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCB dispatcher		    	File: cfe_vendor_xreq.c
    *  
    *  This routine is the main API dispatch for CFE.  User API
    *  calls, via the ROM entry point, get dispatched to routines
    *  in this module.
    *
    *  This version of cfe_xreq is used for vendor extensions to
    *  CFE.
    *
    *  This module looks similar to cfe_iocb_dispatch - it is different
    *  in that the data structure used, cfe_xiocb_t, uses fixed
    *  size field members (specifically, all 64-bits) no matter how
    *  the firmware is compiled.  This ensures a consistent API
    *  interface on any implementation.  When you call CFE
    *  from another program, the entry vector comes here first.
    *
    *  Should the normal cfe_iocb interface change, this one should
    *  be kept the same for backward compatibility reasons.
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
#include "cfe_xiocb.h"
#include "cfe_vendor_xiocb.h"
#include "cfe_error.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#include "cfe_mem.h"
#include "env_subr.h"
#include "cfe.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

/* enum values for various plist types */

#define PLBUF	1		/* iocb_buffer_t */

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

struct cfe_vendor_xcmd_dispatch_s {
    int xplistsize;
    int iplistsize;
    int plisttype;
};


/*  *********************************************************************
    *  Command conversion table 
    *  This table contains useful information for converting
    *  iocbs to xiocbs.
    ********************************************************************* */

const static struct cfe_vendor_xcmd_dispatch_s 
   cfe_vendor_xcmd_dispatch_table[CFE_CMD_VENDOR_MAX-CFE_CMD_VENDOR_USE] = {
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 0 : CFE_CMD_VENDOR_SAMPLE */
};


/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern int cfe_vendor_iocb_dispatch(cfe_vendor_iocb_t *iocb);
extern cfe_int_t cfe_vendor_doxreq(cfe_vendor_xiocb_t *xiocb);

/*  *********************************************************************
    *  cfe_vendor_doxreq(xiocb)
    *  
    *  Process an xiocb request.  This routine converts an xiocb
    *  into an iocb, calls the IOCB dispatcher, converts the results
    *  back into the xiocb, and returns.
    *  
    *  Input parameters: 
    *  	   xiocb - pointer to user xiocb
    *  	   
    *  Return value:
    *  	   command status, <0 if error occured
    ********************************************************************* */

cfe_int_t cfe_vendor_doxreq(cfe_vendor_xiocb_t *xiocb)
{
    const struct cfe_vendor_xcmd_dispatch_s *disp;
    cfe_vendor_iocb_t iiocb;
    cfe_int_t res;

    /*
     * Check for commands codes out of range
     */

    if ((xiocb->xiocb_fcode < CFE_CMD_VENDOR_USE) || 
	(xiocb->xiocb_fcode >= CFE_CMD_VENDOR_MAX)) {
	xiocb->xiocb_status = CFE_ERR_INV_COMMAND;
	return xiocb->xiocb_status;
	}

    /*
     * Check for command codes in range but invalid
     */

    disp = &cfe_vendor_xcmd_dispatch_table[xiocb->xiocb_fcode - CFE_CMD_VENDOR_USE];

    if (disp->xplistsize < 0) {
	xiocb->xiocb_status = CFE_ERR_INV_COMMAND;
	return xiocb->xiocb_status;
	}

    /*
     * Check for invalid parameter list size
     */

    if (disp->xplistsize != xiocb->xiocb_psize) {
	xiocb->xiocb_status = CFE_ERR_INV_PARAM;
	return xiocb->xiocb_status;
	}

    /*
     * Okay, copy parameters into the internal IOCB.
     * First, the fixed header.
     */

    iiocb.iocb_fcode = (unsigned int) xiocb->xiocb_fcode;
    iiocb.iocb_status = (int) xiocb->xiocb_status;
    iiocb.iocb_handle = (int) xiocb->xiocb_handle;
    iiocb.iocb_flags = (unsigned int) xiocb->xiocb_flags;
    iiocb.iocb_psize = (unsigned int) disp->iplistsize;

    /*
     * Now the parameter list 
     */

    switch (disp->plisttype) {
	case PLBUF:
	    iiocb.plist.iocb_buffer.buf_offset = (cfe_offset_t) xiocb->plist.xiocb_buffer.buf_offset;
	    iiocb.plist.iocb_buffer.buf_ptr = (unsigned char *) (uintptr_t) xiocb->plist.xiocb_buffer.buf_ptr;
	    iiocb.plist.iocb_buffer.buf_length = (unsigned int) xiocb->plist.xiocb_buffer.buf_length;
	    iiocb.plist.iocb_buffer.buf_retlen = (unsigned int) xiocb->plist.xiocb_buffer.buf_retlen;
	    iiocb.plist.iocb_buffer.buf_ioctlcmd = (unsigned int) xiocb->plist.xiocb_buffer.buf_ioctlcmd;
	    break;
	}

    /*
     * Do the internal function dispatch
     */

    res = (cfe_int_t) cfe_vendor_iocb_dispatch(&iiocb);

    /*
     * Now convert the parameter list members back
     */

    switch (disp->plisttype) {
	case PLBUF:
	    xiocb->plist.xiocb_buffer.buf_offset = (cfe_uint_t) iiocb.plist.iocb_buffer.buf_offset;
	    xiocb->plist.xiocb_buffer.buf_ptr = (cfe_xptr_t) (uintptr_t) iiocb.plist.iocb_buffer.buf_ptr;
	    xiocb->plist.xiocb_buffer.buf_length = (cfe_uint_t) iiocb.plist.iocb_buffer.buf_length;
	    xiocb->plist.xiocb_buffer.buf_retlen = (cfe_uint_t) iiocb.plist.iocb_buffer.buf_retlen;
	    xiocb->plist.xiocb_buffer.buf_ioctlcmd = (cfe_uint_t) iiocb.plist.iocb_buffer.buf_ioctlcmd;
	    break;
	}

    /*
     * And the fixed header
     */

    xiocb->xiocb_status = (cfe_int_t) iiocb.iocb_status;
    xiocb->xiocb_handle = (cfe_int_t) iiocb.iocb_handle;
    xiocb->xiocb_flags = (cfe_uint_t) iiocb.iocb_flags;

    return xiocb->xiocb_status;
}
