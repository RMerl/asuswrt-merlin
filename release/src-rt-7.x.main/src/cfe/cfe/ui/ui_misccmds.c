/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Miscellaneous commands			File: ui_misccmds.c
    *  
    *  Some small but useful commands
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
#include "cfe_error.h"
#include "cfe_ioctl.h"
#include "cfe_devfuncs.h"
#include "ui_command.h"
#include "cfe.h"


static int ui_cmd_loop(ui_cmdline_t *cmd,int argc,char *argv[]);
#ifdef _FUNCSIM_
static int ui_cmd_exit(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif
static int ui_cmd_console(ui_cmdline_t *cmd,int argc,char *argv[]);

int ui_init_misccmds(void);

int ui_init_misccmds(void)
{

    cmd_addcmd("loop",
	       ui_cmd_loop,
	       NULL,
	       "Loop a command",
	       "loop \"command\" [-count=*]\n"
	       "The 'loop' command causes the specified command or list of commands\n"
	       "to be repeated 'count' times or forever, or until a character is typed",
	       "-count=*;Specifies number of iterations|"
	       "-forever;Loops forever");

#ifdef _FUNCSIM_
    cmd_addcmd("exit",
	       ui_cmd_exit,
	       NULL,
	       "exit from the functional simulator",
	       "exit [n]\n\n"
	       "This command is useful only when running under the functional\n"
	       "simulator.   It causes the simulator to exit and return to the\n"
	       "operating system.  If specified, 'n' will be placed in $4 as a\n"
	       "return code.",
	       "");
#endif

    cmd_addcmd("set console",
	       ui_cmd_console,
	       NULL,
	       "Change the active console device",
	       "set console device-name\n\n"
	       "Changes the console device to the specified device name.  The console\n"
	       "must be a serial-style device.  Be careful not to change the console\n"
	       "to a device that is not connected!",
	       "");

    return 0;
}

static int ui_cmd_loop(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int count = 10;
    char *x;
    int res;
    int forever;

    if (cmd_sw_value(cmd,"-count",&x)) count = atoi(x);
    
    forever = cmd_sw_isset(cmd,"-forever");

    x = cmd_getarg(cmd,0);
    if (!x) return ui_showusage(cmd);

    res = 0;
    while (count || forever) {
	if (console_status()) break;
	res = ui_docommands(x);
	if (res != 0) break;
	count--;
	}

    return res;
}


#ifdef _FUNCSIM_
static int ui_cmd_exit(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int val = 0;
    char *x;

    x = cmd_getarg(cmd,0);
    if (x) val = atoi(x);

    __asm __volatile ("move $4,%0 ; li $2,1 ; syscall 0xca" : "=r"(val));

    return -1;
}
#endif


static int ui_cmd_console(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res;
    char *dev;

    dev = cmd_getarg(cmd,0);
    if (!dev) return -1;

    res = cfe_getdevinfo(dev);
    if (res < 0) {
	xprintf("Device '%s' is not valid\n",dev);
	return CFE_ERR_DEVNOTFOUND;
	}

    if ((res & CFE_DEV_MASK) != CFE_DEV_SERIAL) {
	xprintf("Device '%s' is not the appropriate type to be a console\n",
		dev);
	return CFE_ERR_WRONGDEVTYPE;
	}

    cfe_set_console(dev);

    return 0;
}
