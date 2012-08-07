/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCB definitions				File: cfe_iocb.h
    *  
    *  This module describes CFE's IOCB structure, the main
    *  data structure used to communicate API requests with CFE.
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

#ifndef _CFE_VENDOR_IOCB_H
#define _CFE_VENDOR_IOCB_H

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define CFE_CMD_VENDOR_SAMPLE	(CFE_CMD_VENDOR_USE+0)

#define CFE_CMD_VENDOR_MAX	(CFE_CMD_VENDOR_USE+1)


/*  *********************************************************************
    *  Structures
    ********************************************************************* */

/*
 * Vendor note: The fields in this structure leading up to the
 * union *must* match the ones in cfe_iocb.h
 * 
 * You can declare your own parameter list members here, since
 * only your functions will be using them.
 */


typedef struct cfe_vendor_iocb_s {
    cfe_uint_t iocb_fcode;		/* IOCB function code */
    cfe_int_t  iocb_status;		/* return status */
    cfe_int_t  iocb_handle;		/* file/device handle */
    cfe_uint_t iocb_flags;		/* flags for this IOCB */
    cfe_uint_t iocb_psize;		/* size of parameter list */
    union {
	/* add/replace parameter list here */
	iocb_buffer_t  iocb_buffer;	/* buffer parameters */
    } plist;
} cfe_vendor_iocb_t;


#endif
