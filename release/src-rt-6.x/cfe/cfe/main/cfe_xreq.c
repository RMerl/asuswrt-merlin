/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCB dispatcher		    	File: cfe_xreq.c
    *  
    *  This routine is the main API dispatch for CFE.  User API
    *  calls, via the ROM entry point, get dispatched to routines
    *  in this module.
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


#include "bsp_config.h"
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_queue.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_iocb.h"
#include "cfe_xiocb.h"
#if CFG_VENDOR_EXTENSIONS
#include "cfe_vendor_iocb.h"
#include "cfe_vendor_xiocb.h"
#endif
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
#define PLCPU	2		/* iocb_cpuctl_t */
#define PLMEM	3		/* iocb_meminfo_t */
#define PLENV	4		/* iocb_envbuf_t */
#define PLINP	5		/* iocb_inpstat_t */
#define PLTIM	6		/* iocb_time_t */
#define PLINF   7		/* iocb_fwinfo_t */
#define PLEXIT  8		/* iocb_exitstat_t */

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

struct cfe_xcmd_dispatch_s {
    int xplistsize;
    int iplistsize;
    int plisttype;
};


/*  *********************************************************************
    *  Command conversion table 
    *  This table contains useful information for converting
    *  iocbs to xiocbs.
    ********************************************************************* */

const static struct cfe_xcmd_dispatch_s cfe_xcmd_dispatch_table[CFE_CMD_MAX] = {
    {sizeof(xiocb_fwinfo_t), sizeof(iocb_fwinfo_t), PLINF},	/* 0 : CFE_CMD_FW_GETINFO */
    {sizeof(xiocb_exitstat_t),sizeof(iocb_exitstat_t),PLEXIT},  /* 1 : CFE_CMD_FW_RESTART */
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 2 : CFE_CMD_FW_BOOT */
    {sizeof(xiocb_cpuctl_t), sizeof(iocb_cpuctl_t), PLCPU},	/* 3 : CFE_CMD_FW_CPUCTL */
    {sizeof(xiocb_time_t),   sizeof(iocb_time_t),   PLTIM},	/* 4 : CFE_CMD_FW_GETTIME */
    {sizeof(xiocb_meminfo_t),sizeof(iocb_meminfo_t),PLMEM},	/* 5 : CFE_CMD_FW_MEMENUM */
    {0,		    	     0,                     0},		/* 6 : CFE_CMD_FW_FLUSHCACHE */
    {-1,		     0,                     0},		/* 7 : */
    {-1,		     0,                     0},		/* 8 : */
    {0,			     0,                     0},		/* 9 : CFE_CMD_DEV_GETHANDLE */
    {sizeof(xiocb_envbuf_t), sizeof(iocb_envbuf_t), PLENV},	/* 10 : CFE_CMD_DEV_ENUM */
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 11 : CFE_CMD_DEV_OPEN_*/
    {sizeof(xiocb_inpstat_t),sizeof(iocb_inpstat_t),PLINP},	/* 12 : CFE_CMD_DEV_INPSTAT */
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 13 : CFE_CMD_DEV_READ */
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 14 : CFE_CMD_DEV_WRITE */
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 15 : CFE_CMD_DEV_IOCTL */
    {0,			     0,                     0},		/* 16 : CFE_CMD_DEV_CLOSE */
    {sizeof(xiocb_buffer_t), sizeof(iocb_buffer_t), PLBUF},	/* 17 : CFE_CMD_DEV_GETINFO */
    {-1,		     0,                     0},		/* 18 : */
    {-1,		     0,                     0},		/* 19 : */
    {sizeof(xiocb_envbuf_t), sizeof(iocb_envbuf_t), PLENV},	/* 20 : CFE_CMD_ENV_ENUM */
    {-1,		     0,                     0},		/* 21 : */
    {sizeof(xiocb_envbuf_t), sizeof(iocb_envbuf_t), PLENV},	/* 22 : CFE_CMD_ENV_GET */
    {sizeof(xiocb_envbuf_t), sizeof(iocb_envbuf_t), PLENV},	/* 23 : CFE_CMD_ENV_SET */
    {sizeof(xiocb_envbuf_t), sizeof(iocb_envbuf_t), PLENV},	/* 24 : CFE_CMD_ENV_DEL */
    {-1,		     0,                     0},		/* 25 : */
    {-1,		     0,                     0},		/* 26 : */
    {-1,		     0,                     0},		/* 27 : */
    {-1,		     0,                     0},		/* 28 : */
    {-1,		     0,                     0},		/* 29 : */
    {-1,		     0,                     0},		/* 30 : */
    {-1,		     0,                     0}		/* 31 : */
};


/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern int cfe_iocb_dispatch(cfe_iocb_t *iocb);
cfe_int_t cfe_doxreq(cfe_xiocb_t *xiocb);
#if CFG_VENDOR_EXTENSIONS
extern cfe_int_t cfe_vendor_doxreq(cfe_vendor_xiocb_t *xiocb);
#endif

/*  *********************************************************************
    *  cfe_doxreq(xiocb)
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

cfe_int_t cfe_doxreq(cfe_xiocb_t *xiocb)
{
    const struct cfe_xcmd_dispatch_s *disp;
    cfe_iocb_t iiocb;
    cfe_int_t res;

    /*
     * Call out to customer-specific IOCBs.  Customers may choose
     * to implement their own XIOCBs directly, or go through their own
     * translation layer (xiocb->iocb like CFE does) to insulate 
     * themselves from IOCB changes in the future.
     */
    if (xiocb->xiocb_fcode >= CFE_CMD_VENDOR_USE) {
#if CFG_VENDOR_EXTENSIONS
	return cfe_vendor_doxreq((cfe_vendor_xiocb_t *)xiocb);
#else
	return CFE_ERR_INV_COMMAND;
#endif
	}

    /*
     * Check for commands codes out of range
     */

    if ((xiocb->xiocb_fcode < 0) || (xiocb->xiocb_fcode >= CFE_CMD_MAX)) {
	xiocb->xiocb_status = CFE_ERR_INV_COMMAND;
	return xiocb->xiocb_status;
	}

    /*
     * Check for command codes in range but invalid
     */

    disp = &cfe_xcmd_dispatch_table[xiocb->xiocb_fcode];

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
	case PLCPU:
	    iiocb.plist.iocb_cpuctl.cpu_number = (unsigned int) xiocb->plist.xiocb_cpuctl.cpu_number;
	    iiocb.plist.iocb_cpuctl.cpu_command = (unsigned int) xiocb->plist.xiocb_cpuctl.cpu_command;
	    iiocb.plist.iocb_cpuctl.start_addr = (unsigned long) xiocb->plist.xiocb_cpuctl.start_addr;
	    iiocb.plist.iocb_cpuctl.gp_val = (unsigned long) xiocb->plist.xiocb_cpuctl.gp_val;
	    iiocb.plist.iocb_cpuctl.sp_val = (unsigned long) xiocb->plist.xiocb_cpuctl.sp_val;
	    iiocb.plist.iocb_cpuctl.a1_val = (unsigned long) xiocb->plist.xiocb_cpuctl.a1_val;
	    break;
	case PLMEM:
	    iiocb.plist.iocb_meminfo.mi_idx  = (int) xiocb->plist.xiocb_meminfo.mi_idx;
	    iiocb.plist.iocb_meminfo.mi_type = (int) xiocb->plist.xiocb_meminfo.mi_type;
	    iiocb.plist.iocb_meminfo.mi_addr = (unsigned long long) xiocb->plist.xiocb_meminfo.mi_addr;
	    iiocb.plist.iocb_meminfo.mi_size = (unsigned long long) xiocb->plist.xiocb_meminfo.mi_size;
	    break;
	case PLENV:
	    iiocb.plist.iocb_envbuf.enum_idx = (int) xiocb->plist.xiocb_envbuf.enum_idx;
	    iiocb.plist.iocb_envbuf.name_ptr = (unsigned char *) (uintptr_t) xiocb->plist.xiocb_envbuf.name_ptr;
	    iiocb.plist.iocb_envbuf.name_length = (int) xiocb->plist.xiocb_envbuf.name_length;
	    iiocb.plist.iocb_envbuf.val_ptr = (unsigned char *) (uintptr_t) xiocb->plist.xiocb_envbuf.val_ptr;
	    iiocb.plist.iocb_envbuf.val_length = (int) xiocb->plist.xiocb_envbuf.val_length;
	    break;
	case PLINP:
	    iiocb.plist.iocb_inpstat.inp_status = (int) xiocb->plist.xiocb_inpstat.inp_status;
	    break;
	case PLTIM:
	    iiocb.plist.iocb_time.ticks = (long long) xiocb->plist.xiocb_time.ticks;
	    break;
	case PLINF:
	    break;
	case PLEXIT:
	    iiocb.plist.iocb_exitstat.status = (long long) xiocb->plist.xiocb_exitstat.status;
	    break;
	}

    /*
     * Do the internal function dispatch
     */

    res = (cfe_int_t) cfe_iocb_dispatch(&iiocb);

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
	case PLCPU:
	    xiocb->plist.xiocb_cpuctl.cpu_number = (cfe_uint_t) iiocb.plist.iocb_cpuctl.cpu_number;
	    xiocb->plist.xiocb_cpuctl.cpu_command = (cfe_uint_t) iiocb.plist.iocb_cpuctl.cpu_command;
	    xiocb->plist.xiocb_cpuctl.start_addr = (cfe_uint_t) iiocb.plist.iocb_cpuctl.start_addr;
	    break;
	case PLMEM:
	    xiocb->plist.xiocb_meminfo.mi_idx = (cfe_int_t) iiocb.plist.iocb_meminfo.mi_idx;
	    xiocb->plist.xiocb_meminfo.mi_type = (cfe_int_t) iiocb.plist.iocb_meminfo.mi_type;
	    xiocb->plist.xiocb_meminfo.mi_addr = (cfe_int64_t) iiocb.plist.iocb_meminfo.mi_addr;
	    xiocb->plist.xiocb_meminfo.mi_size = (cfe_int64_t) iiocb.plist.iocb_meminfo.mi_size;
	    break;
	case PLENV:
	    xiocb->plist.xiocb_envbuf.enum_idx = (cfe_int_t) iiocb.plist.iocb_envbuf.enum_idx;
	    xiocb->plist.xiocb_envbuf.name_ptr = (cfe_xptr_t) (uintptr_t) iiocb.plist.iocb_envbuf.name_ptr;
	    xiocb->plist.xiocb_envbuf.name_length = (cfe_int_t) iiocb.plist.iocb_envbuf.name_length;
	    xiocb->plist.xiocb_envbuf.val_ptr = (cfe_xptr_t) (uintptr_t) iiocb.plist.iocb_envbuf.val_ptr;
	    xiocb->plist.xiocb_envbuf.val_length = (cfe_int_t) iiocb.plist.iocb_envbuf.val_length;
	    break;
	case PLINP:
	    xiocb->plist.xiocb_inpstat.inp_status = (cfe_int_t) iiocb.plist.iocb_inpstat.inp_status;
	    break;
	case PLTIM:
	    xiocb->plist.xiocb_time.ticks = (cfe_int_t) iiocb.plist.iocb_time.ticks;
	    break;
	case PLINF:
	    xiocb->plist.xiocb_fwinfo.fwi_version = iiocb.plist.iocb_fwinfo.fwi_version;
	    xiocb->plist.xiocb_fwinfo.fwi_totalmem = iiocb.plist.iocb_fwinfo.fwi_totalmem;
	    xiocb->plist.xiocb_fwinfo.fwi_flags = iiocb.plist.iocb_fwinfo.fwi_flags;
	    xiocb->plist.xiocb_fwinfo.fwi_boardid = iiocb.plist.iocb_fwinfo.fwi_boardid;
	    xiocb->plist.xiocb_fwinfo.fwi_bootarea_va = iiocb.plist.iocb_fwinfo.fwi_bootarea_va;
	    xiocb->plist.xiocb_fwinfo.fwi_bootarea_pa = iiocb.plist.iocb_fwinfo.fwi_bootarea_pa;
	    xiocb->plist.xiocb_fwinfo.fwi_bootarea_size = iiocb.plist.iocb_fwinfo.fwi_bootarea_size;
	    xiocb->plist.xiocb_fwinfo.fwi_reserved1 = iiocb.plist.iocb_fwinfo.fwi_reserved1;
	    xiocb->plist.xiocb_fwinfo.fwi_reserved2 = iiocb.plist.iocb_fwinfo.fwi_reserved2;
	    xiocb->plist.xiocb_fwinfo.fwi_reserved3 = iiocb.plist.iocb_fwinfo.fwi_reserved3;
	    break;
	case PLEXIT:
	    xiocb->plist.xiocb_exitstat.status = (cfe_int_t) iiocb.plist.iocb_exitstat.status;
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
