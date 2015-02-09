/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Test commands				File: cfe_tests.c
    *  
    *  A temporary sandbox for misc test routines and commands.
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
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"
#include "cfe_ioctl.h"

#include "cfe_error.h"

#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "cfe_mem.h"

int ui_init_testcmds(void);
static int ui_cmd_timertest(ui_cmdline_t *cmd,int argc,char *argv[]);
#if CFG_DOWNLOAD
static int ui_cmd_config1250(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif

int ui_init_testcmds(void)
{

    cmd_addcmd("test timer",
	       ui_cmd_timertest,
	       NULL,
	       "Test the timer",
	       "test timer",
	       "");
#if CFG_DOWNLOAD
    cmd_addcmd("test bcm1250",
	       ui_cmd_config1250,
	       NULL,
	       "Configure a bcm1250 as a PCI device",
	       "test bcm1250 device-name [file-name]\n\n"
	       "Download code to the specified 1250-based PCI device",
	       ""
	);
#endif

    return 0;
}




#if CFG_DOWNLOAD

#include "net_ebuf.h"
#include "net_api.h"
#include "addrspace.h"
#include "cfe_loader.h"
extern cfe_loadargs_t cfe_loadargs;

/* Staging buffer for downloaded files.  See ui_flash for rationale. */
#define FILE_STAGING_BUFFER		(1024*1024)	/* 1MB line */
#define FILE_STAGING_BUFFER_SIZE	(4048*1024)

/* Base and bound of compiled-in downloadable image */
extern void download_start(void), download_end(void);

static int ui_cmd_config1250(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *tok;
    int fh;
    uint8_t *base;
    int len;
    int res;

    tok = cmd_getarg(cmd, 0);
    if (!tok) {
	return ui_showusage(cmd);
	}

    if (argc > 1) {
	char *fname;
	cfe_loadargs_t *la = &cfe_loadargs;
	int res;
    
	fname = cmd_getarg(cmd, 1);
	if (!fname) {
	    return ui_showusage(cmd);
	    }

	/* Get the file using tftp and read it into a known location. */

	/* (From ui_flash.c):
         * We can't allocate the space from the heap to store the 
	 * file to download, because the heap may not be big enough.
	 * So, grab some unallocated memory at the 1MB line (we could
	 * also calculate something, but this will do for now).
	 * We assume the downloadable file will be no bigger than 4MB.
	 */

	/* Fill out the loadargs */

	la->la_flags = LOADFLG_NOISY;

	la->la_device = (char *) net_getparam(NET_DEVNAME);
	la->la_filesys = "tftp";
	la->la_filename = fname;

	/* Temporary: use a fixed memory buffer. */
	la->la_address = KERNADDR(FILE_STAGING_BUFFER);
	la->la_maxsize = FILE_STAGING_BUFFER_SIZE;;
	la->la_flags |= LOADFLG_SPECADDR;

	la->la_options = NULL;

	xprintf("Loader:raw Filesys:%s Dev:%s File:%s Options:%s\n",
		la->la_filesys, la->la_device, la->la_filename, la->la_options);

	res = cfe_load_program("raw", la);
	if (res < 0) {
	    xprintf("Could not load %s\n", fname);
	    return res;
	    }

	base = (uint8_t *)(la->la_address);
	len = res;
	}
    else {
	/* This code casts a function pointer to a byte pointer, which is
	   suspect in ANSI C but appears to work with the gnu MIPS tool
	   chain.  Changing the externs to, e.g., uint8_t * does not work
	   with PIC code (generates relocation errors). */

	base = (uint8_t *)download_start;
	len  = (uint8_t *)download_end - (uint8_t *)download_start;
	}
    xprintf("PCI download: base %p len %d\n", base, len);

    fh = cfe_open(tok);
    if (fh < 0) {
	xprintf("Could not open device: %s\n", cfe_errortext(fh));
	return fh;
	}

    res = cfe_write(fh, base, len);
    if (res != 0) {
	xprintf("Could not download device: %s\n", cfe_errortext(res));
	}

    cfe_close(fh);

    return res;
}
#endif /* CFG_DOWNLOAD */




static int ui_cmd_timertest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int64_t t;

    t = cfe_ticks;

    while (!console_status()) {
	cfe_sleep(CFE_HZ);
	if (t != cfe_ticks) {
	    xprintf("Time is %lld\n",cfe_ticks);
	    t = cfe_ticks;	    
	    }
	}

    return 0;
}
