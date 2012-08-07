/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Program and file loading URLs		File: url.c
    *  
    *  Functions to process URLs for loading software.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001
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
    *     and retain this copyright notice and list of conditions as 
    *     they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation. Neither the "Broadcom 
    *     Corporation" name nor any trademark or logo of Broadcom 
    *     Corporation may be used to endorse or promote products 
    *     derived from this software without the prior written 
    *     permission of Broadcom Corporation.
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

#include "bsp_config.h"
#include "cfe_loader.h"
#include "cfe_autoboot.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_fileops.h"
#include "cfe_error.h"

#include "net_ebuf.h"
#include "net_ether.h"
#include "net_api.h"

#include "ui_command.h"

#include "url.h"

static long getaddr(char *str)
{
    /* 
     * hold on to your lunch, this is really, really bad! 
     * Make 64-bit addresses expressed as 8-digit numbers
     * sign extend automagically.  Saves typing, but is very
     * gross.  Not very portable, either.
     */
    int longaddr = 0;
    long newaddr;

    longaddr = strlen(str);
    if (memcmp(str,"0x",2) == 0) longaddr -= 2;
    longaddr = (longaddr > 8) ? 1 : 0;

    if (longaddr) newaddr = (long) xtoq(str);
    else newaddr = (long) xtoi(str);

    return newaddr;
}

static int process_oldstyle(char *str,
			    ui_cmdline_t *cmd,
			    cfe_loadargs_t *la)
{
    char *file;
    char *devname;
    char *filesys = NULL;
    char *loader = la->la_loader;
    char *x;
    int info;
    char *colon;

    colon = strchr(str,':');

    if (!colon) {
	return CFE_ERR_DEVNOTFOUND;
	}

    devname = str;	/* will be used to check protocol later */
    *colon = '\0';
    file = colon + 1;	/* Ugly, we might put the colon back! */

    /*
     * Try to determine the load protocol ("filesystem")
     * first by using the command line, and
     * if not that try to figure it out automagically
     */

    if (cmd_sw_isset(cmd,"-fatfs")) filesys = "fat";
    if (cmd_sw_isset(cmd,"-tftp"))  filesys = "tftp";
    if (cmd_sw_isset(cmd,"-rawfs")) filesys = "raw";
#if CFG_TCP && CFG_HTTPFS
    if (cmd_sw_isset(cmd,"-http"))  filesys = "http";
#endif
    if (cmd_sw_value(cmd,"-fs",&x)) filesys = x;

    /*
     * Automagic configuration
     */

    /*
     * Determine the device type from the "host" name.  If we look
     * up the host name and it appears to be an invalid CFE device
     * name, then it's probably a TFTP host name.
     *
     * This is where we guess based on the device type what
     * sort of load method we're going to use.
     */

    info = devname ? cfe_getdevinfo(devname) : -1;
    if (info >= 0) {
	switch (info & CFE_DEV_MASK) {
	    case CFE_DEV_NETWORK:
		if (!filesys) filesys = "tftp";
		if (!loader)  loader  = "raw";
		break;
	    case CFE_DEV_DISK:
		if (!filesys) filesys = "raw";
		if (!loader)  loader  = "raw";
		break;
	    case CFE_DEV_FLASH:
		if (!filesys) filesys = "raw";
		if (!loader)  loader  = "raw";
		break;
	    case CFE_DEV_SERIAL:
		if (!filesys) filesys = "raw";
		if (!loader)  loader  = "srec";
		break;
	    default:
		break;
	    }
	la->la_device   = devname;
	la->la_filename = file;
	}
    else {
	/*
	 * It's probably a network boot.  Default to TFTP
	 * if not overridden
	 */
#if CFG_NETWORK
	la->la_device = (char *) net_getparam(NET_DEVNAME);
#else
	la->la_device = NULL;
#endif
	*colon = ':';			/* put the colon back */
	la->la_filename = devname;
	if (!filesys) filesys = "tftp";
	if (!loader)  loader  = "raw";
	}

    /*
     * Remember our file system and loader.
     */

    la->la_filesys = filesys;
    la->la_loader = loader;

    return 0;
}


#if CFG_URLS
static int process_url(char *str,
		       ui_cmdline_t *cmd,
		       cfe_loadargs_t *la)
{
    char *p;
    char *protocol;
    int idx,len;
    int network = 0;
    const fileio_dispatch_t *fdisp;

    /*
     * All URLs have the string "://" in them somewhere
     * If that's not there, try the old syntax.
     */

    len = strlen(str);
    p = str;

    for (idx = 0; idx < len-3; idx++) {
	if (memcmp(p,"://",3) == 0) break;
	p++;
	}

    if (idx == (len-3)) {
	return process_oldstyle(str,cmd,la);
	}

    /* 
     * Break the string apart into protocol, host, file
     */

    protocol = str;
    *p = '\0';
    p += 3;

    /*
     * Determine if this is a network loader.  If that is true,
     * the meaning of the "device" field is different.  Ugh.
     */

    fdisp = cfe_findfilesys(protocol);
    if (fdisp && (fdisp->loadflags & FSYS_TYPE_NETWORK)) network = 1;

    /*
     * Depending on the protocol we parse the file name one of two ways:
     *
     *      protocol://hostname/filename
     *
     * For network devices:
     *
     *      the "device" is the current Ethernet device.
     *      The filename is the //hostname/filename from the URL.
     *
     * For non-network devices:
     *
     *      The "device" is the CFE device name from the URL 'hostname' field
     *      The filename is from the URL filename field.
     */   

    la->la_filesys = protocol;

    if (network) {
#if CFG_NETWORK
	la->la_device = (char *) net_getparam(NET_DEVNAME);
#else
	la->la_device = NULL;
#endif
	la->la_filename = p;
	}
    else {
	la->la_device = p;
	p = strchr(p,'/');
	if (p) {
	    *p++ = '\0';
	    la->la_filename = p;
	    }
	else {
	    la->la_filename = NULL;
	    }
	}

    if (!la->la_loader) la->la_loader = "raw";

    return 0;
}
#endif



int ui_process_url(char *url,ui_cmdline_t *cmd,cfe_loadargs_t *la)
{
    int res;
    char *x;

    /*
     * Skip leading whitespace
     */

    while (*url && ((*url == ' ') || (*url == '\t'))) url++;

    /*
     * Process command-line switches to determine the loader stack
     */

    la->la_loader = NULL;
    if (cmd_sw_isset(cmd,"-elf"))  la->la_loader = "elf";
    if (cmd_sw_isset(cmd,"-srec")) la->la_loader = "srec";
    if (cmd_sw_isset(cmd,"-raw"))  la->la_loader = "raw";
    if (cmd_sw_value(cmd,"-loader",&x)) la->la_loader = x;

#if CFG_ZLIB || CFG_LZMA
    if (cmd_sw_isset(cmd,"-z")) {
	la->la_flags |= LOADFLG_COMPRESSED;
	}
#endif

    /*
     * Parse the file name into its pieces.
     */

#if CFG_URLS
    res = process_url(url,cmd,la);
    if (res < 0) return res;
#else
    res = process_oldstyle(url,cmd,la);
    if (res < 0) return res;
#endif


    /*
     * This is used only by "boot" and "load" - to avoid this code
     * don't include these switches in the command table.
     */

    if (cmd_sw_value(cmd,"-max",&x)) {
	la->la_maxsize = atoi(x);
	}

    if (cmd_sw_value(cmd,"-addr",&x)) {
	la->la_address = getaddr(x);
	la->la_flags |= LOADFLG_SPECADDR;
	}

    if (cmd_sw_isset(cmd,"-noclose")) {
	la->la_flags |= LOADFLG_NOCLOSE;
	}


    return 0;    
}
