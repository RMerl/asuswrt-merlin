/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  UART Test commands			File: ui_test_uart.c
    *  
    *  Some commands to test the uart device interface.
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

#include "ui_command.h"

int ui_init_uarttestcmds(void);

static int ui_cmd_uarttest(ui_cmdline_t *cmd,int argc,char *argv[]);

int ui_init_uarttestcmds(void)
{
    cmd_addcmd("test uart",
	       ui_cmd_uarttest,
	       NULL,
	       "Echo characters to a UART",
	       "test uart [devname]",
	       "");
    return 0;
}


static int ui_cmd_uarttest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int fd;
    char *x;
    char ch;
    int res;
    char buffer[64];

    x = cmd_getarg(cmd,0);
    if (!x) return ui_showusage(cmd);

    fd = cfe_open(x);
    if (fd < 0) {
	ui_showerror(fd,"could not open %s",x);
	return fd;
	}

    printf("Device open.  Stuff you type here goes there.  Type ~ to exit.\n");
    for (;;) {
	if (console_status()) {
	    console_read(&ch,1);
	    res = cfe_write(fd,(uint8_t *)&ch,1);
	    if (res < 0) break;
	    if (ch == '~') break;
	    }
	if (cfe_inpstat(fd)) {
	    res = cfe_read(fd,(uint8_t *)buffer,sizeof(buffer));
	    if (res > 0) console_write(buffer,res);
	    if (res < 0) break;
	    }
	POLL();
	}

    cfe_close(fd);
    return 0;
}
