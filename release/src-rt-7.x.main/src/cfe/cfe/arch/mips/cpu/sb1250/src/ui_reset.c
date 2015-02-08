/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  RESET command				File: ui_reset.c
    *  
    *  Commands to reset the CPU
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

#include "cfe_timer.h"

#include "cfe_error.h"
#include "cfe_console.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_resetcmds(void);
static int ui_cmd_reset(ui_cmdline_t *cmd,int argc,char *argv[]);


/*  *********************************************************************
    *  Data
    ********************************************************************* */


/*  *********************************************************************
    *  ui_init_resetcmds()
    *  
    *  Add RESET-specific commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_resetcmds(void)
{
    cmd_addcmd("reset",
	       ui_cmd_reset,
	       NULL,
	       "Reset the system.",
	       "reset [-yes] -softreset|-cpu|-unicpu1|-unicpu0|-sysreset",
	       "-yes;Don't ask for confirmation|"
	       "-softreset;Soft reset of the entire chip|"
	       "-cpu;Reset the CPUs|"
	       "-unicpu1;Reset to uniprocessor using CPU1|"
	       "-unicpu0;Reset to uniprocessor using CPU0|"
	       "-sysreset;Full system reset");



    return 0;
}





/*  *********************************************************************
    *  ui_cmd_reset(cmd,argc,argv)
    *  
    *  RESET command.
    *  
    *  Input parameters: 
    *  	   cmd - command structure
    *  	   argc,argv - parameters
    *  	   
    *  Return value:
    *  	   -1 if error occured.  Does not return otherwise
    ********************************************************************* */

static int ui_cmd_reset(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint64_t data;
    uint64_t olddata;
    int confirm = 1;
    char str[50];

    data = SBREADCSR(A_SCD_SYSTEM_CFG) & ~M_SYS_SB_SOFTRES;
    olddata = data;

    if (cmd_sw_isset(cmd,"-yes")) confirm = 0;

    if (cmd_sw_isset(cmd,"-softreset")) data |= M_SYS_SB_SOFTRES;

    if (cmd_sw_isset(cmd,"-unicpu0")) data |= M_SYS_UNICPU0;
    else if (cmd_sw_isset(cmd,"-unicpu1")) data |= M_SYS_UNICPU1;

    if (cmd_sw_isset(cmd,"-sysreset")) data |= M_SYS_SYSTEM_RESET;

    if (cmd_sw_isset(cmd,"-cpu")) data |= (M_SYS_CPU_RESET_0 | M_SYS_CPU_RESET_1);

    if (data == olddata) {		/* no changes to reset pins were specified */
	return ui_showusage(cmd);
	}

    if (confirm) {
	console_readline("Are you sure you want to reset? ",str,sizeof(str));
	if ((str[0] != 'Y') && (str[0] != 'y')) return -1;
	}

    SBWRITECSR(A_SCD_SYSTEM_CFG,data);

    /* should not return */

    return -1;
}
