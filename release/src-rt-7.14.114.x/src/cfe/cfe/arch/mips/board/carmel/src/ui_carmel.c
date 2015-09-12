/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  carmel-specific commands		File: ui_carmel.c
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

#include "lib_physio.h"

#include "cfe_timer.h"

#include "cfe_error.h"
#include "cfe_console.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"

#include "carmel.h"

#include "env_subr.h"
#include "carmel_env.h"

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_carmelcmds(void);

static int ui_cmd_mreset(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_msetup(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_msetenv(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_mprintenv(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_munsetenv(ui_cmdline_t *cmd,int argc,char *argv[]);

/*  *********************************************************************
    *  Data
    ********************************************************************* */


/*  *********************************************************************
    *  ui_init_carmelcmds()
    *  
    *  Add carmel-specific commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_carmelcmds(void)
{
     cmd_addcmd("monterey reset",
	       ui_cmd_mreset,
	       NULL,
	       "Reset the Monterey Subsystem",
	       "monterey reset [0|1]\n\n"
		"This command resets the board that the Carmel is attached to.  For\n"
		"example, if Carmel is attached to a Monterey board, the Monterey\n"
		"FPGA is reset.  You can specify an absolute value for the reset pin\n"
		"or type 'monterey reset' without a parameter to pulse the reset line.",
		"");

    cmd_addcmd("monterey setenv",
	       ui_cmd_msetenv,
	       NULL,
	       "Set a Monterey environment variable.",
	       "monterey setenv [-p] varname value\n\n"
	       "This command sets an environment variable.  By default, an environment variable\n"
	       "is stored only in memory and will not be retained across system restart.",
	       "-p;Store environment variable permanently in the NVRAM device, if present");

    cmd_addcmd("monterey printenv",
	       ui_cmd_mprintenv,
	       NULL,
	       "Display the Monterey environment variables",
	       "monterey printenv\n\n"
	       "This command prints a table of the Monterey environment variables and their\n"
	       "current values.",
	       "");

    cmd_addcmd("monterey unsetenv",
	       ui_cmd_munsetenv,
	       NULL,
	       "Delete a Monterey environment variable.",
	       "monterey unsetenv varname\n\n"
	       "This command deletes a Monterey environment variable from memory and also \n"
	       "removes it from the NVRAM device (if present).",
	       "");

    cmd_addcmd("monterey setup",
	       ui_cmd_msetup,
	       NULL,
	       "Interactive setup for Monterey variables.",
	       "monterey setup\n\n"
	       "This command prompts for the important Monterey environment variables and\n"
	       "stores them in the ID EEPROM\n",
	       "");

    return 0;

}



static int ui_cmd_mreset(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *x;
    int val;

    if ((x = cmd_getarg(cmd,0))) {
	val = atoi(x);
	if (val) {
	    SBWRITECSR(A_GPIO_PIN_SET,M_GPIO_MONTEREY_RESET);
	    }
	else {
	    SBWRITECSR(A_GPIO_PIN_CLR,M_GPIO_MONTEREY_RESET);
	    }
	printf("Monterey reset pin set to %d\n",val ? 1 : 0);
	}
    else {
	SBWRITECSR(A_GPIO_PIN_CLR,M_GPIO_MONTEREY_RESET);
	cfe_sleep(2);
	SBWRITECSR(A_GPIO_PIN_SET,M_GPIO_MONTEREY_RESET);
	printf("Monterey has been reset.\n");
	}

    return 0;
}






static int ui_cmd_mprintenv(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char varname[80];
    char value[256];
    int varlen,vallen;
    int idx;

    xprintf("Variable Name        Value\n");
    xprintf("-------------------- --------------------------------------------------\n");

    idx = 0;
    for (;;) {
	varlen = sizeof(varname);
	vallen = sizeof(value);
	if (carmel_enumenv(idx,varname,&varlen,value,&vallen) < 0) break;
	xprintf("%20s %s\n",varname,value);
	idx++;
	}

    return 0;
    
}

static int ui_cmd_msetenv(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *varname;
    char *value;
    int roflag = ENV_FLG_NORMAL;
    int res;

    varname = cmd_getarg(cmd,0);

    if (!varname) {
	return ui_showusage(cmd);
	}

    value = cmd_getarg(cmd,1);
    if (!value) {
	return ui_showusage(cmd);
	}

    if (!cmd_sw_isset(cmd,"-p")) {
	roflag = ENV_FLG_BUILTIN;	/* just in memory, not NVRAM */
	}

    if ((res = carmel_setenv(varname,value,roflag)) == 0) {
	if (roflag != ENV_FLG_BUILTIN) carmel_saveenv();
	}
    else {
	return ui_showerror(res,"Could not set Monterey environment variable '%s'",
			    varname);
	}

    return 0;
}


static int ui_cmd_munsetenv(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *varname;
    int res;
    int type;

    varname = cmd_getarg(cmd,0);

    if (!varname) {
	return ui_showusage(cmd);
	}

    type = carmel_envtype(varname);

    if ((res = carmel_delenv(varname)) == 0) {
	if ((type >= 0) && (type != ENV_FLG_BUILTIN)) carmel_saveenv();
	}
    else {
	return ui_showerror(res,"Could not delete Monterey environment variable '%s'",
			    varname);
	}

    return 0;
}


#define MSETUP_UCASE	1
#define MSETUP_YESNO	2

static int msetup_getone(char *prompt,char *envvar,char *defval,int flg)
{
    char buffer[100];
    char pbuf[100];
    char *curval;

    curval = carmel_getenv(envvar);
    if (!curval) curval = defval;

    sprintf(pbuf,"%s [%s]: ",prompt,curval);

    for (;;) {
	console_readline(pbuf,buffer,sizeof(buffer));
	if (buffer[0] == 0) break;		/* default */
	if (flg & (MSETUP_UCASE|MSETUP_YESNO)) strupr(buffer);
	if (flg & MSETUP_YESNO) {
	    if (strcmp(buffer,"YES") == 0) break;
	    if (strcmp(buffer,"NO") == 0) break;
	    printf("You must enter 'YES' or 'NO'.\n");
	    continue;
	    }
	break;
	}
	

    if (buffer[0] == 0) {
	carmel_setenv(envvar,defval,0);
	}
    else {
	carmel_setenv(envvar,buffer,0);
	}

    return 0;
    
}

static int ui_cmd_msetup(ui_cmdline_t *cmd,int argc,char *argv[])
{

    msetup_getone("Board type (MONTEREY,LITTLESUR,BIGSUR)","M_BOARDTYPE","MONTEREY",MSETUP_UCASE);
    msetup_getone("FPGA Autoload (YES,NO)","M_FPGALOAD","YES",MSETUP_YESNO);
    msetup_getone("FPGA GPIO bits convoluted (YES,NO)","M_FPGACONVOLUTED","NO",
		  MSETUP_YESNO);

    carmel_saveenv();
    return 0;
}
