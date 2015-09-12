/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Environment commands			File: ui_envcmds.c
    *  
    *  User interface for environment variables
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


int ui_init_envcmds(void);
static int ui_cmd_setenv(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_printenv(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_unsetenv(ui_cmdline_t *cmd,int argc,char *argv[]);



static int ui_cmd_printenv(ui_cmdline_t *cmd,int argc,char *argv[])
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
	if (env_enum(idx,varname,&varlen,value,&vallen) < 0) break;
	xprintf("%20s %s\n",varname,value);
	idx++;
	}

    return 0;
    
}

static int ui_cmd_setenv(ui_cmdline_t *cmd,int argc,char *argv[])
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

    if (cmd_sw_isset(cmd,"-ro")) {
	roflag = ENV_FLG_READONLY;
	}

    if ((res = env_setenv(varname,value,roflag)) == 0) {
	if (roflag != ENV_FLG_BUILTIN) env_save();
	}
    else {
	return ui_showerror(res,"Could not set environment variable '%s'",
			    varname);
	}

    return 0;
}


static int ui_cmd_unsetenv(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *varname;
    int res;
    int type;

    varname = cmd_getarg(cmd,0);

    if (!varname) {
	return ui_showusage(cmd);
	}

    type = env_envtype(varname);

    if ((res = env_delenv(varname)) == 0) {
	if ((type >= 0) && (type != ENV_FLG_BUILTIN)) env_save();
	}
    else {
	return ui_showerror(res,"Could not delete environment variable '%s'",
			    varname);
	}

    return 0;
}



int ui_init_envcmds(void)
{

    cmd_addcmd("setenv",
	       ui_cmd_setenv,
	       NULL,
	       "Set an environment variable.",
	       "setenv [-ro] [-p] varname value\n\n"
	       "This command sets an environment variable.  By default, an environment variable\n"
	       "is stored only in memory and will not be retained across system restart.",
	       "-p;Store environment variable permanently in the NVRAM device, if present|"
	       "-ro;Causes variable to be read-only\n" 
	       "(cannot be changed in the future, implies -p)");

    cmd_addcmd("printenv",
	       ui_cmd_printenv,
	       NULL,
	       "Display the environment variables",
	       "printenv\n\n"
	       "This command prints a table of the environment variables and their\n"
	       "current values.",
	       "");

    cmd_addcmd("unsetenv",
	       ui_cmd_unsetenv,
	       NULL,
	       "Delete an environment variable.",
	       "unsetenv varname\n\n"
	       "This command deletes an environment variable from memory and also \n"
	       "removes it from the NVRAM device (if present).",
	       "");

    return 0;
}
