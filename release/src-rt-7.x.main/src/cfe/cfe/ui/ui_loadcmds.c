/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Program Loader commands			File: ui_loadcmds.c
    *  
    *  User interface for program loader
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

#include "ui_command.h"
#include "cfe.h"

#include "net_ebuf.h"
#include "net_ether.h"
#include "net_api.h"

#include "cfe_fileops.h"
#include "cfe_boot.h"

#include "bsp_config.h"
#include "cfe_loader.h"
#include "cfe_autoboot.h"

#include "url.h"


int ui_init_loadcmds(void);
static int ui_cmd_load(ui_cmdline_t *cmd,int argc,char *argv[]);

#if CFG_NETWORK
static int ui_cmd_save(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif

#if CFG_AUTOBOOT
static int ui_cmd_autoboot(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif	/* CFG_AUTOBOOT */
static int ui_cmd_boot(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_batch(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_go(ui_cmdline_t *cmd,int argc,char *argv[]);


extern cfe_loadargs_t cfe_loadargs;

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



int ui_init_loadcmds(void)
{	

#if CFG_NETWORK
    cmd_addcmd("save",
	       ui_cmd_save,
	       NULL,
	       "Save a region of memory to a remote file via TFTP",
	       "save [-options] host:filename startaddr length\n\n",
	       "");
#endif

    cmd_addcmd("load",
	       ui_cmd_load,
	       NULL,
	       "Load an executable file into memory without executing it",
	       "load [-options] host:filename|dev:filename\n\n"
	       "This command loads an executable file into memory, but does not\n"
	       "execute it.  It can be used for loading data files, overlays or\n"
	       "other programs needed before the 'boot' command is used.  By\n"
	       "default, 'load' will load a raw binary at virtual address 0x20000000.",
	       "-elf;Load the file as an ELF executable|"
	       "-srec;Load the file as ASCII S-records|"
	       "-raw;Load the file as a raw binary|"
#if CFG_ZLIB || CFG_LZMA
	       "-z;Load compessed file|"
#endif
	       "-loader=*;Specify CFE loader name|"
	       "-tftp;Load the file using the TFTP protocol|"
	       "-fatfs;Load the file from a FAT file system|"
	       "-rawfs;Load the file from an unformatted file system|"
#if CFG_TCP && CFG_HTTPFS
	       "-http;Load the file using the HTTP protocol|"
#endif
               "-fs=*;Specify CFE file system name|"
	       "-max=*;Specify the maximum number of bytes to load (raw only)|"
	       "-addr=*;Specify the load address (hex) (raw only)");

    cmd_addcmd("boot",
	       ui_cmd_boot,
	       NULL,
	       "Load an executable file into memory and execute it",
	       "boot [-options] host:filename|dev:filename\n\n"
	       "This command loads and executes a program from a boot device\n"
	       "By default, 'boot' will load a raw binary at virtual \n"
	       "address 0x20000000 and then jump to that address",
	       "-elf;Load the file as an ELF executable|"
	       "-srec;Load the file as ASCII S-records|"
	       "-raw;Load the file as a raw binary|"
#if CFG_ZLIB || CFG_LZMA
	       "-z;Load compessed file|"
#endif
	       "-loader=*;Specify CFE loader name|"
	       "-tftp;Load the file using the TFTP protocol|"
	       "-fatfs;Load the file from a FAT file system|"
	       "-rawfs;Load the file from an unformatted file system|"
#if CFG_TCP && CFG_HTTPFS
	       "-http;Load the file using the HTTP protocol|"
#endif
               "-fs=*;Specify CFE file system name|"
	       "-max=*;Specify the maximum number of bytes to load (raw only)|"
	       "-addr=*;Specify the load address (hex) (raw only)|"
	       "-noclose;Don't close network link before executing program");

    cmd_addcmd("go",
	       ui_cmd_go,
	       NULL,
	       "Start a previously loaded program.",
	       "go [address]\n\n"
	       "The 'go' command will start a program previously loaded with \n"
	       "the 'load' command.  You can override the start address by"
	       "specifying it as a parameter to the 'go' command.",
	       "-noclose;Don't close network link before executing program");

    cmd_addcmd("batch",
	       ui_cmd_batch,
	       NULL,
	       "Load a batch file into memory and execute it",
	       "batch [-options] host:filename|dev:filename\n\n"
	       "This command loads and executes a batch file from a boot device",
#if CFG_ZLIB || CFG_LZMA
	       "-z;Load compessed file|"
#endif
#if CFG_ROMBOOT
	       "-raw;Load the file as a raw binary|"
	       "-max=*;Specify the maximum number of bytes to load (raw only)|"
	       "-addr=*;Specify the load address (hex) (raw only)|"	
#endif
	       "-tftp;Load the file using the TFTP protocol|"
	       "-fatfs;Load the file from a FAT file system|"
	       "-rawfs;Load the file from an unformatted file system|"
               "-fs=*;Specify CFE file system name");

#if CFG_AUTOBOOT
    cmd_addcmd("autoboot",
	       ui_cmd_autoboot,
	       NULL,
	       "Automatic system bootstrap.",
	       "autoboot [dev]\n\n"
	       "The 'autoboot' command causes an automatic system bootstrap from\n"
	       "a predefined list of devices and boot files.  This list is \n"
	       "specific to the board and port of CFE.  To try autobooting from\n"
	       "a specific device, you can specify the CFE device name on the command line.",
	       "-forever;Loop over devices until boot is successful|"
	       "-interruptible;Scan console between devices, drop to prompt if key pressed");
#endif	/* CFG_AUTOBOOT */

    return 0;
}


#if CFG_AUTOBOOT
static int ui_cmd_autoboot(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res;
    char *x;
    int flags = 0;

    if (cmd_sw_isset(cmd,"-forever")) flags |= CFE_AUTOFLG_TRYFOREVER;
    if (cmd_sw_isset(cmd,"-interruptible")) flags |= CFE_AUTOFLG_POLLCONSOLE;

    x = cmd_getarg(cmd,0);
    res = cfe_autoboot(x,flags);

    return res;
}
#endif	/* CFG_AUTOBOOT */

static int ui_cmd_go(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *arg;

    arg = cmd_getarg(cmd,0);
    if (arg) {
	cfe_loadargs.la_entrypt = getaddr(arg);
	}

    if (cmd_sw_isset(cmd,"-noclose")) {
	cfe_loadargs.la_flags |= LOADFLG_NOCLOSE;
	}

    cfe_go(&cfe_loadargs);

    return 0;
}



static int ui_cmd_bootcommon(ui_cmdline_t *cmd,int argc,char *argv[],int flags)
{
    int res;
    char *arg;
    cfe_loadargs_t *la = &cfe_loadargs;
    char copy[200];

    la->la_flags = flags;

    arg = cmd_getarg(cmd,0);	
    strncpy(copy,arg,sizeof(copy));

    if (!arg) {
	xprintf("No program name specified\n");
	return -1;
	}

    res = ui_process_url(arg,cmd,la);
    if (res < 0) return res;
	
    /*
     * Pick up the remaining command line parameters for use as
     * arguments to the loaded program.
     */

    la->la_options = cmd_getarg(cmd,1);

    /*
     * Note: we might not come back here if we really launch the program.
     */

    xprintf("Loader:%s Filesys:%s Dev:%s File:%s Options:%s\n",
	    la->la_loader,la->la_filesys,la->la_device,la->la_filename,la->la_options);

    res = cfe_boot(la->la_loader,la);

    /*	
     * Give the bad news.
     */

    if (res < 0) xprintf("Could not load %s: %s\n",copy,cfe_errortext(res));

    return res;
}


static int ui_cmd_load(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int flags = LOADFLG_NOISY;

    return ui_cmd_bootcommon(cmd,argc,argv,flags);
}




static int ui_cmd_boot(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int flags = LOADFLG_NOISY | LOADFLG_EXECUTE;

    return ui_cmd_bootcommon(cmd,argc,argv,flags);
}

static int ui_cmd_batch(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int flags = LOADFLG_NOISY | LOADFLG_EXECUTE | LOADFLG_BATCH;

    return ui_cmd_bootcommon(cmd,argc,argv,flags);
}

#if CFG_NETWORK
static int ui_cmd_save(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *x;
    uint8_t *start,*end;
    int len;
    char *fname;
    int res;

    fname = cmd_getarg(cmd,0);

    if ((x = cmd_getarg(cmd,1))) {
	start = (uint8_t *) getaddr(x);
	}
    else {
	return ui_showusage(cmd);
	}

    if ((x = cmd_getarg(cmd,2))) {
	len = xtoi(x);
	}
    else {
	return ui_showusage(cmd);
	}

    end = start+len;

    res = cfe_savedata("tftp","",fname,start,end);

    if (res < 0) {
	return ui_showerror(res,"Could not dump data to network");
	}
    else {
	xprintf("%d bytes written to %s\n",res,fname);
	}

    return 0;
}
#endif
