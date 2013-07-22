/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SOC Display functions			File: ui_soccmds.c
    *  
    *  UI functions for examining SOC registers
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



#include "sbmips.h"
#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_error.h"
#include "cfe_console.h"

#include "ui_command.h"
#include "cfe.h"

#include "socregs.h"

#include "sb1250_regs.h"
#include "sb1250_socregs.inc"

#include "exchandler.h"


static int ui_cmd_soc(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_socagents(ui_cmdline_t *cmd,int argc,char *argv[]);

int ui_init_soccmds(void);


/*
 * Command formats:
 *
 * show soc agentname instance subinstance
 *
 * show soc mac 		Show all MACs
 * show soc mac 0 		Show just MAC0
 * show soc macdma 0 tx0	Show just TX channel 0 of MAC DMA 0
 */

static void ui_showreg(const socreg_t *reg,int verbose)
{
    char buffer[100];
    char number[30];
    char *ptr = buffer;
    uint64_t value = 0;
    int res;

    ptr += sprintf(ptr,"%s",socagents[reg->reg_agent]);
    if (reg->reg_inst[0] != '*') {
	ptr += sprintf(ptr," %s",reg->reg_inst);
	}
    if (reg->reg_subinst[0] != '*') {
	ptr += sprintf(ptr," %s",reg->reg_subinst);
	}
    ptr += sprintf(ptr," %s",reg->reg_descr);

    res = mem_peek(&value,PHYS_TO_K1(reg->reg_addr),MEM_QUADWORD);

//    value = *((volatile uint64_t *) PHYS_TO_K1(reg->reg_addr));

    if (res == 0) sprintf(number,"%016llX",value); 
    else sprintf(number,"N/A N/A N/A N/A ");

    xprintf("%30s  0x%08X  %4s_%4s_%4s_%4s\n",
	    buffer,reg->reg_addr,
	    &number[0],&number[4],&number[8],&number[12]);


    if (verbose && (reg->reg_printfunc)) {
	xprintf("    ");
	(*(reg->reg_printfunc))(reg,value);
	xprintf("\n");
	}

    
}

static int ui_cmd_soc(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *agent = NULL;
    char *inst = NULL;
    char *subinst = NULL;
    const socreg_t *reg;
    int verbose;

    reg = socregs;

    agent   = cmd_getarg(cmd,0);
    inst    = cmd_getarg(cmd,1);
    subinst = cmd_getarg(cmd,2);

    if (!cmd_sw_isset(cmd,"-all")) {
	if (!agent) return ui_showusage(cmd);
	}

    if (cmd_sw_isset(cmd,"-v")) verbose = 1;
    else verbose = 0;

    xprintf("Register Name                   Address     Value\n");
    xprintf("------------------------------  ----------  -------------------\n");

    while (reg->reg_descr) {
	if (!agent || (strcmpi(agent,socagents[reg->reg_agent]) == 0)) {
	    /* Handle the case of subinstances of something we have only one of */
	    if (reg->reg_inst[0] != '*') {
		if ((!inst || (strcmpi(inst,reg->reg_inst) == 0)) &&
		    (!subinst || (strcmpi(subinst,reg->reg_subinst) == 0))) {
		    ui_showreg(reg,verbose);
		    }
		}
	    else {
		if (!inst || (strcmpi(inst,reg->reg_subinst) == 0)) {
		    ui_showreg(reg,verbose);
		    }
		}
	    }	
	reg++;
	}

    return 0;
}

static int ui_cmd_socagents(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char **aptr;

    aptr = socagents;
    
    xprintf("Available SOC agents: ");
    while (*aptr) {
	xprintf("%s",*aptr);
	aptr++;
	if (*aptr) xprintf(", ");
	}
    xprintf("\n");

    return 0;
}


int ui_init_soccmds(void)
{
    cmd_addcmd("show soc",
	       ui_cmd_soc,
	       NULL,
	       "Display SOC register contents",
	       "show soc agentname [instance [section]]\n\n"
	       "Display register values for SOC registers.\n",
	       "-v;Verbose information: Display register fields where available|"
	       "-all;Display all registers in all agents");

    cmd_addcmd("show agents",
	       ui_cmd_socagents,
	       NULL,
	       "Display list of SOC agents",
	       "show agents\n\n"
	       "Display the names of the available SOC agents that can be examined\n"
	       "using the 'show soc' command",
	       "");

    return 0;
}
