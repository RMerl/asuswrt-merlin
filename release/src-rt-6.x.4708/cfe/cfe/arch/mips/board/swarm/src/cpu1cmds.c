/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  CPU1 Test commands			File: cpu1cmds.c
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

#include "cfe_error.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

int ui_init_cpu1cmds(void);
static int ui_cmd_cpu1(ui_cmdline_t *cmd,int argc,char *argv[]);
extern int cfe_iocb_dispatch(cfe_iocb_t *iocb);


int ui_init_cpu1cmds(void)
{
    cmd_addcmd("cpu1",
	       ui_cmd_cpu1,
	       NULL,
	       "Controls a test program running on CPU1",
	       "cpu1 start|stop",
	       "-addr=*;Specifies a start address for CPU1|"
	       "-a1=*;Specifies an initial value for the A1 register|"
	       "-gp=*;Specifies an initial value for the GP register|"
	       "-sp=*;Specifies an initial value for the SP register");

    return 0;
}


extern void cpu1proc(void);

static int ui_cmd_cpu1(ui_cmdline_t *cmd,int argc,char *argv[])
{
    cfe_iocb_t iocb;
    int res = 0;
    char *a;
    char *x;

    a = cmd_getarg(cmd,0);
    if (!a) a = "";

    iocb.iocb_fcode = CFE_CMD_FW_CPUCTL;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_cpuctl_t);

    if (strcmp(a,"start") == 0) {
	iocb.plist.iocb_cpuctl.cpu_number = 1;
	iocb.plist.iocb_cpuctl.cpu_command = CFE_CPU_CMD_START;

	if (cmd_sw_value(cmd,"-a1",&x)) iocb.plist.iocb_cpuctl.a1_val = (cfe_uint_t) xtoq(x);
	else iocb.plist.iocb_cpuctl.a1_val = 0xFEEDFACE;

	if (cmd_sw_value(cmd,"-gp",&x)) iocb.plist.iocb_cpuctl.gp_val = (cfe_uint_t) xtoq(x);
	else iocb.plist.iocb_cpuctl.gp_val = 0xFEEDFACE;

	if (cmd_sw_value(cmd,"-sp",&x)) iocb.plist.iocb_cpuctl.sp_val = (cfe_uint_t) xtoq(x);
	else iocb.plist.iocb_cpuctl.sp_val = 0x12345678;

	iocb.plist.iocb_cpuctl.start_addr = (cfe_uint_t) cpu1proc;
	if (cmd_sw_value(cmd,"-addr",&x)) iocb.plist.iocb_cpuctl.start_addr = (cfe_uint_t) xtoq(x);

	xprintf("Starting CPU 1 at %p\n",iocb.plist.iocb_cpuctl.start_addr);
	res = cfe_iocb_dispatch(&iocb);
	}
    else if (strcmp(a,"stop") == 0) {
	iocb.plist.iocb_cpuctl.cpu_number = 1;
	iocb.plist.iocb_cpuctl.cpu_command = CFE_CPU_CMD_STOP;
	iocb.plist.iocb_cpuctl.start_addr = 0;
	xprintf("Stopping CPU 1\n");
	res = cfe_iocb_dispatch(&iocb);
	}
    else {
	xprintf("Invalid CPU1 command: use 'cpu1 stop' or 'cpu1 start'\n");
	return -1;
	}

    printf("Result %d\n",res);
    return res;
}
