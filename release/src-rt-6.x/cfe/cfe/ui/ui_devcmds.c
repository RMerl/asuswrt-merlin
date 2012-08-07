/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Device Commands				File: ui_devcmds.c
    *  
    *  User interface for the device manager
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
#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"


extern queue_t cfe_devices;

int ui_init_devcmds(void);

static int ui_cmd_devnames(ui_cmdline_t *cmd,int argc,char *argv[]);


int ui_cmd_devnames(ui_cmdline_t *cmd,int argc,char *argv[])
{
    queue_t *qb;
    cfe_device_t *dev;

    xprintf("Device Name          Description\n");
    xprintf("-------------------  ---------------------------------------------------------\n");

    for (qb = cfe_devices.q_next; qb != &cfe_devices; qb = qb->q_next) {
	dev = (cfe_device_t *) qb;

	xprintf("%19s  %s\n",dev->dev_fullname,
		dev->dev_description);

	}

    return 0;
}


int ui_init_devcmds(void)
{
    cmd_addcmd("show devices",
	       ui_cmd_devnames,
	       NULL,
	       "Display information about the installed devices.",
	       "show devices\n\n"
	       "This command displays the names and descriptions of the devices\n"
	       "CFE is configured to support.",
	       "");

    return 0;
}
