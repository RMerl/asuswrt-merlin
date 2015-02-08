/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCB dispatcher		    	File: cfe_iocb_dispatch.c
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
#include "initdata.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define HV	1			/* handle valid */

#ifndef CFG_BOARD_ID
#define CFG_BOARD_ID 0
#endif

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

cfe_devctx_t *cfe_handle_table[CFE_MAX_HANDLE];

extern void _cfe_flushcache(int);

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int cfe_iocb_dispatch(cfe_iocb_t *iocb);
void cfe_device_poll(void *);

#if CFG_MULTI_CPUS
extern int altcpu_cmd_start(uint64_t,uint64_t *);
extern int altcpu_cmd_stop(uint64_t);
#endif

/*  *********************************************************************
    *  Dispatch table
    ********************************************************************* */

struct cfe_cmd_dispatch_s {
    int plistsize;
    int flags;
    int (*func)(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
};


static int cfe_cmd_fw_getinfo(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_fw_restart(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_fw_boot(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_fw_cpuctl(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_fw_gettime(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_fw_memenum(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_fw_flushcache(cfe_devctx_t *ctx,cfe_iocb_t *iocb);

static int cfe_cmd_dev_gethandle(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_enum(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_open(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_inpstat(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_read(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_write(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_ioctl(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_close(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_dev_getinfo(cfe_devctx_t *ctx,cfe_iocb_t *iocb);

static int cfe_cmd_env_enum(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_env_get(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_env_set(cfe_devctx_t *ctx,cfe_iocb_t *iocb);
static int cfe_cmd_env_del(cfe_devctx_t *ctx,cfe_iocb_t *iocb);

const static struct cfe_cmd_dispatch_s cfe_cmd_dispatch_table[CFE_CMD_MAX] = {
    {sizeof(iocb_fwinfo_t), 0,	cfe_cmd_fw_getinfo},		/* 0 : CFE_CMD_FW_GETINFO */
    {sizeof(iocb_exitstat_t),0,	cfe_cmd_fw_restart},		/* 1 : CFE_CMD_FW_RESTART */
    {sizeof(iocb_buffer_t), 0,	cfe_cmd_fw_boot},		/* 2 : CFE_CMD_FW_BOOT */
    {sizeof(iocb_cpuctl_t), 0,	cfe_cmd_fw_cpuctl},		/* 3 : CFE_CMD_FW_CPUCTL */
    {sizeof(iocb_time_t),   0,	cfe_cmd_fw_gettime},		/* 4 : CFE_CMD_FW_GETTIME */
    {sizeof(iocb_meminfo_t),0,  cfe_cmd_fw_memenum},		/* 5 : CFE_CMD_FW_MEMENUM */
    {0,		    	    0,  cfe_cmd_fw_flushcache},		/* 6 : CFE_CMD_FW_FLUSHCACHE */
    {-1,		    0,	NULL},				/* 7 : */
    {-1,		    0,	NULL},				/* 8 : */
    {0,			    0,	cfe_cmd_dev_gethandle},		/* 9 : CFE_CMD_DEV_GETHANDLE */
    {sizeof(iocb_envbuf_t), 0,	cfe_cmd_dev_enum},		/* 10 : CFE_CMD_DEV_ENUM */
    {sizeof(iocb_buffer_t), 0,	cfe_cmd_dev_open},		/* 11 : CFE_CMD_DEV_OPEN */
    {sizeof(iocb_inpstat_t),HV,	cfe_cmd_dev_inpstat},		/* 12 : CFE_CMD_DEV_INPSTAT */
    {sizeof(iocb_buffer_t), HV,	cfe_cmd_dev_read},		/* 13 : CFE_CMD_DEV_READ */
    {sizeof(iocb_buffer_t), HV,	cfe_cmd_dev_write},		/* 14 : CFE_CMD_DEV_WRITE */
    {sizeof(iocb_buffer_t), HV,	cfe_cmd_dev_ioctl},		/* 15 : CFE_CMD_DEV_IOCTL */
    {0,			    HV,	cfe_cmd_dev_close},		/* 16 : CFE_CMD_DEV_CLOSE */
    {sizeof(iocb_buffer_t), 0,	cfe_cmd_dev_getinfo},		/* 17 : CFE_CMD_DEV_GETINFO */
    {-1,		    0,	NULL},				/* 18 : */
    {-1,		    0,	NULL},				/* 19 : */
    {sizeof(iocb_envbuf_t), 0,	cfe_cmd_env_enum},		/* 20 : CFE_CMD_ENV_ENUM */
    {-1,		    0,	NULL},				/* 21 : */
    {sizeof(iocb_envbuf_t), 0,	cfe_cmd_env_get},		/* 22 : CFE_CMD_ENV_GET */
    {sizeof(iocb_envbuf_t), 0,	cfe_cmd_env_set},		/* 23 : CFE_CMD_ENV_SET */
    {sizeof(iocb_envbuf_t), 0,	cfe_cmd_env_del},		/* 24 : CFE_CMD_ENV_DEL */
    {-1,		    0,	NULL},				/* 25 : */
    {-1,		    0,	NULL},				/* 26 : */
    {-1,		    0,	NULL},				/* 27 : */
    {-1,		    0,	NULL},				/* 28 : */
    {-1,		    0,	NULL},				/* 29 : */
    {-1,		    0,	NULL},				/* 30 : */
    {-1,		    0,  NULL}				/* 31 : */
};

/*  *********************************************************************
    *  IOCB dispatch routines
    ********************************************************************* */

void cfe_device_poll(void *x)
{
    int idx;
    cfe_devctx_t **ctx = cfe_handle_table;

    for (idx = 0; idx < CFE_MAX_HANDLE; idx++,ctx++) {
	if ((*ctx) && ((*ctx)->dev_dev->dev_dispatch->dev_poll)) {
	    (*ctx)->dev_dev->dev_dispatch->dev_poll(*ctx,cfe_ticks);
	    }
	}
}

int cfe_iocb_dispatch(cfe_iocb_t *iocb)
{
    const struct cfe_cmd_dispatch_s *disp;
    int res;
    cfe_devctx_t *ctx;

    /*
     * Check for commands codes out of range
     */

    if ((iocb->iocb_fcode < 0) || (iocb->iocb_fcode >= CFE_CMD_MAX)) {
	iocb->iocb_status = CFE_ERR_INV_COMMAND;
	return iocb->iocb_status;
	}

    /*
     * Check for command codes in range but invalid
     */

    disp = &cfe_cmd_dispatch_table[iocb->iocb_fcode];

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

static int cfe_newhandle(void)
{
    int idx;

    for (idx = 0; idx < CFE_MAX_HANDLE; idx++) {
	if (cfe_handle_table[idx] == NULL) break;
	}

    if (idx == CFE_MAX_HANDLE) return -1;

    return idx;
}


/*  *********************************************************************
    *  Implementation routines for each IOCB function
    ********************************************************************* */

static int cfe_cmd_fw_getinfo(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    iocb_fwinfo_t *info = &iocb->plist.iocb_fwinfo;

    info->fwi_version = (CFE_VER_MAJOR << 16) |
	(CFE_VER_MINOR << 8) |
	(CFE_VER_BUILD);
    info->fwi_totalmem = ((cfe_int64_t) mem_totalsize) << 10;
    info->fwi_flags = 
#ifdef __long64
	CFE_FWI_64BIT |
#else
	CFE_FWI_32BIT |
#endif
#if CFG_EMBEDDED_PIC
	CFE_FWI_RELOC |
#endif
#if !CFG_RUNFROMKSEG0
	CFE_FWI_UNCACHED |
#endif
#if CFG_MULTI_CPUS
	CFE_FWI_MULTICPU |
#endif
#ifdef _VERILOG_
	CFE_FWI_RTLSIM |
#endif
#ifdef _FUNCSIM_
	CFE_FWI_FUNCSIM |
#endif
	0;

    info->fwi_boardid = CFG_BOARD_ID;
    info->fwi_bootarea_pa = (cfe_int64_t) mem_bootarea_start;
    info->fwi_bootarea_va = BOOT_START_ADDRESS;
    info->fwi_bootarea_size = (cfe_int64_t) mem_bootarea_size;
    info->fwi_reserved1 = 0;
    info->fwi_reserved2 = 0;
    info->fwi_reserved3 = 0;

    return CFE_OK;
}

static int cfe_cmd_fw_restart(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    if (iocb->iocb_flags & CFE_FLG_WARMSTART) {
	cfe_warmstart(iocb->plist.iocb_exitstat.status);
	}
    else {
	cfe_restart();
	}

    /* should not get here */

    return CFE_OK;
}

static int cfe_cmd_fw_boot(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    return CFE_ERR_INV_COMMAND;		/* not implemented yet */
}

static int cfe_cmd_fw_cpuctl(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
#if CFG_MULTI_CPUS
    int res;
    uint64_t startargs[4];

    switch (iocb->plist.iocb_cpuctl.cpu_command) {
	case CFE_CPU_CMD_START:

	    startargs[0] = iocb->plist.iocb_cpuctl.start_addr;
	    startargs[1] = iocb->plist.iocb_cpuctl.sp_val;
	    startargs[2] = iocb->plist.iocb_cpuctl.gp_val;
	    startargs[3] = iocb->plist.iocb_cpuctl.a1_val;

	    res = altcpu_cmd_start(iocb->plist.iocb_cpuctl.cpu_number,
				   startargs);
	    break;	
	case CFE_CPU_CMD_STOP:
	    res = altcpu_cmd_stop(iocb->plist.iocb_cpuctl.cpu_number);
	    break;
	default:
	    res = CFE_ERR_INV_PARAM;
	}

    return res;
#else
    return CFE_ERR_INV_COMMAND;
#endif
}

static int cfe_cmd_fw_gettime(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    POLL();

    iocb->plist.iocb_time.ticks = cfe_ticks;

    return CFE_OK;
}

static int cfe_cmd_fw_memenum(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int type;
    int res;
    uint64_t addr,size;

    res = cfe_arena_enum(iocb->plist.iocb_meminfo.mi_idx,
			 &type,
			 &addr,
			 &size,
			 (iocb->iocb_flags & CFE_FLG_FULL_ARENA) ? TRUE : FALSE);

    iocb->plist.iocb_meminfo.mi_addr = addr;
    iocb->plist.iocb_meminfo.mi_size = size;
    iocb->plist.iocb_meminfo.mi_type = type;

    if (res == 0) {
	if (type == MEMTYPE_DRAM_AVAILABLE) {
	    iocb->plist.iocb_meminfo.mi_type = CFE_MI_AVAILABLE;
	    }
	else {
	    iocb->plist.iocb_meminfo.mi_type = CFE_MI_RESERVED;
	    }
	}

    return res;
}

static int cfe_cmd_fw_flushcache(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    _cfe_flushcache(iocb->iocb_flags);
    return CFE_OK;
}

static int cfe_cmd_dev_enum(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    return CFE_ERR_INV_COMMAND;
}

static int cfe_cmd_dev_gethandle(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    switch (iocb->iocb_flags) {
	case CFE_STDHANDLE_CONSOLE:
	    if (console_handle == -1) return CFE_ERR_DEVNOTFOUND;
	    iocb->iocb_handle = console_handle;
	    return CFE_OK;
	    break;
	default:
	    return CFE_ERR_INV_PARAM;
	}
}

static int cfe_cmd_dev_open(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int h;
    cfe_device_t *dev;
    char devname[64];
    int res;

    /*
     * Get device name
     */

    xstrncpy(devname,(char *)iocb->plist.iocb_buffer.buf_ptr,sizeof(devname));

    /*
     * Find device in device table
     */

    dev = cfe_finddev(devname);
    if (!dev) return CFE_ERR_DEVNOTFOUND;

    /*
     * Fail if someone else already has the device open
     */

    if (dev->dev_opencount > 0) return CFE_ERR_DEVOPEN;

    /*
     * Generate a new handle
     */

    h = cfe_newhandle();
    if (h < 0) return CFE_ERR_NOMEM;

    /*
     * Allocate a context
     */

    ctx = (cfe_devctx_t *) KMALLOC(sizeof(cfe_devctx_t),0);
    if (ctx == NULL) return CFE_ERR_NOMEM;

    /*
     * Fill in the context
     */

    ctx->dev_dev = dev;
    ctx->dev_softc = dev->dev_softc;
    ctx->dev_openinfo = NULL;

    /*
     * Call driver's open func
     */

    res = dev->dev_dispatch->dev_open(ctx);

    if (res != 0) {
	KFREE(ctx);
	return res;
	}

    /*
     * Increment refcnt and save handle
     */

    dev->dev_opencount++;
    cfe_handle_table[h] = ctx;
    iocb->iocb_handle = h;

    /*
     * Success!
     */

    return CFE_OK;
}

static int cfe_cmd_dev_inpstat(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int status;

    status = ctx->dev_dev->dev_dispatch->dev_inpstat(ctx,&(iocb->plist.iocb_inpstat));

    return status;
}

static int cfe_cmd_dev_read(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int status;

    status = ctx->dev_dev->dev_dispatch->dev_read(ctx,&(iocb->plist.iocb_buffer));

    return status;
}

static int cfe_cmd_dev_write(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int status;

    status = ctx->dev_dev->dev_dispatch->dev_write(ctx,&(iocb->plist.iocb_buffer));

    return status;
}

static int cfe_cmd_dev_ioctl(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int status;

    status = ctx->dev_dev->dev_dispatch->dev_ioctl(ctx,&(iocb->plist.iocb_buffer));

    return status;
}

static int cfe_cmd_dev_close(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    /*
     * Call device close function
     */

    ctx->dev_dev->dev_dispatch->dev_close(ctx);

    /*
     * Decrement refcnt
     */

    ctx->dev_dev->dev_opencount--;

    /*
     * Wipe out handle
     */

    cfe_handle_table[iocb->iocb_handle] = NULL;

    /*
     * Release device context
     */

    KFREE(ctx);

    return CFE_OK;
}

static int cfe_cmd_dev_getinfo(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    cfe_device_t *dev;
    char devname[64];
    char *x;

    /*
     * Get device name
     */

    xstrncpy(devname,(char *)iocb->plist.iocb_buffer.buf_ptr,sizeof(devname));

    /*
     * Find device in device table
     */

    if ((x = strchr(devname,':'))) *x = '\0';
    dev = cfe_finddev(devname);
    if (!dev) return CFE_ERR_DEVNOTFOUND;

    /*
     * Return device class
     */

    iocb->plist.iocb_buffer.buf_devflags = dev->dev_class;

    return CFE_OK;
}

static int cfe_cmd_env_enum(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int vallen,namelen,res;

    namelen = iocb->plist.iocb_envbuf.name_length;
    vallen  = iocb->plist.iocb_envbuf.val_length;

    res = env_enum(iocb->plist.iocb_envbuf.enum_idx,
		   (char *)iocb->plist.iocb_envbuf.name_ptr,
		   &namelen,
		   (char *)iocb->plist.iocb_envbuf.val_ptr,
		   &vallen);

    if (res < 0) return CFE_ERR_ENVNOTFOUND;

    return CFE_OK;
}


static int cfe_cmd_env_get(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    char *env;

    env = env_getenv((char *)iocb->plist.iocb_envbuf.name_ptr);

    if (env == NULL) return CFE_ERR_ENVNOTFOUND;

    xstrncpy((char *)iocb->plist.iocb_envbuf.val_ptr,
	     env,
	     iocb->plist.iocb_envbuf.val_length);

    return CFE_OK;
}

static int cfe_cmd_env_set(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int res;
    int flg;


    flg = (iocb->iocb_flags & CFE_FLG_ENV_PERMANENT) ? 
	ENV_FLG_NORMAL : ENV_FLG_BUILTIN;

    res = env_setenv((char *)iocb->plist.iocb_envbuf.name_ptr,
		     (char *)iocb->plist.iocb_envbuf.val_ptr,
		     flg);

    if (res == 0) {
	if (iocb->iocb_flags & CFE_FLG_ENV_PERMANENT) res = env_save();
	}

    if (res < 0) return res;

    return CFE_OK;
}

static int cfe_cmd_env_del(cfe_devctx_t *ctx,cfe_iocb_t *iocb)
{
    int res;

    res = env_delenv((char *)iocb->plist.iocb_envbuf.name_ptr);

    return res;
}
