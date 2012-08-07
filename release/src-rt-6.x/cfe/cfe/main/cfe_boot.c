/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  OS bootstrap				File: cfe_boot.c
    *  
    *  This module handles OS bootstrap stuff
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

#include "cfe_error.h"

#include "env_subr.h"
#include "cfe.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_api.h"
#include "cfe_fileops.h"
#include "cfe_bootblock.h"
#include "bsp_config.h"
#include "cfe_boot.h"

#include "cfe_loader.h"

#if CFG_UI
extern int ui_docommands(char *buf);
#endif

/*  *********************************************************************
    *  data
    ********************************************************************* */

const char *bootvar_device = "BOOT_DEVICE";
const char *bootvar_file   = "BOOT_FILE";
const char *bootvar_flags  = "BOOT_FLAGS";

cfe_loadargs_t cfe_loadargs;

/*  *********************************************************************
    *  splitpath(path,devname,filename)
    *  
    *  Split a path name (a boot path, in the form device:filename)
    *  into its parts
    *  
    *  Input parameters: 
    *  	   path - pointer to path string
    *  	   devname - receives pointer to device name
    *  	   filename - receives pointer to file name
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void splitpath(char *path,char **devname,char **filename)
{
    char *x;

    *devname = NULL;
    *filename = NULL;

    x = strchr(path,':');

    if (!x) {
	*devname = NULL;	/* path consists of device name */
	*filename = path;
	}
    else {
	*x++ = '\0';		/* have both device and file name */
	*filename = x;
	*devname = path;
	}
}


/*  *********************************************************************
    *  cfe_go(la)
    *  
    *  Starts a previously loaded program.  cfe_loadargs.la_entrypt
    *  must be set to the entry point of the program to be started
    *  
    *  Input parameters: 
    *  	   la - loader args
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

void cfe_go(cfe_loadargs_t *la)
{
    if (la->la_entrypt == 0) {
	xprintf("No program has been loaded.\n");
	return;
	}

#if CFG_NETWORK    
    if (!(la->la_flags & LOADFLG_NOCLOSE)) {
	if (net_getparam(NET_DEVNAME)) {
	    xprintf("Closing network.\n");
	    net_uninit();
	    }
	}
#endif

    xprintf("Starting program at 0x%p\n",la->la_entrypt);

    cfe_start(la->la_entrypt);
}


/*  *********************************************************************
    *  cfe_boot(la)
    *  
    *  Bootstrap the system.
    *  
    *  Input parameters: 
    *  	   la - loader arguments
    *  	   
    *  Return value:
    *  	   error, or does not return
    ********************************************************************* */
int cfe_boot(char *ldrname,cfe_loadargs_t *la)
{
    int res;

    la->la_entrypt = 0;

    if (la->la_flags & LOADFLG_NOISY) {
	xprintf("Loading: ");
	}

    res = cfe_load_program(ldrname,la);

    if (res < 0) {
	if (la->la_flags & LOADFLG_NOISY) {
	    xprintf("Failed.\n");
	    }
	return res;
	}

    /*	
     * Special case: If loading a batch file, just do the commands here
     * and return.  For batch files we don't want to set up the
     * environment variables.
     */

    if (la->la_flags & LOADFLG_BATCH) {
#if CFG_UI
	ui_docommands((char *) la->la_entrypt);
#endif
	return 0;
	}

    /*
     * Otherwise set up for running a real program.
     */

    if (la->la_flags & LOADFLG_NOISY) {
	xprintf("Entry at 0x%p\n",la->la_entrypt);
	}

    /*
     * Set up the environment variables for the bootstrap
     */

    if (la->la_device) {
	env_setenv(bootvar_device,la->la_device,ENV_FLG_BUILTIN);
	}
    else {
	env_delenv(bootvar_device);
	}

    if (la->la_filename) {
	env_setenv(bootvar_file,la->la_filename,ENV_FLG_BUILTIN);
	}
    else {
	env_delenv(bootvar_file);
	}

    if (la->la_options) {
	env_setenv(bootvar_flags,la->la_options,ENV_FLG_BUILTIN);
	}
    else {
	env_delenv(bootvar_flags);
	}

    /*
     * Banzai!  Run the program.
     */

    if ((la->la_flags & LOADFLG_EXECUTE) &&
	(la->la_entrypt != 0)) {
	cfe_go(la);
	}

    return 0;
}
