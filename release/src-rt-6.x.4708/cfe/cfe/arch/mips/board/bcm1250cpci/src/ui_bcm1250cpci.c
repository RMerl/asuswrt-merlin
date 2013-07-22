/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SENTOSA-specific commands        File: ui_sentosa.c
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

#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_smbus.h"
#include "sb1250_scd.h"

#include "bcm1250cpci.h"


extern void ui_init_cpu1cmds(void);
extern int ui_init_memtestcmds(void);
extern void ui_init_usbcmds(void);

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_bcm1250cpcicmds(void);
static int ui_bcm1250cpci_setvxwenv(ui_cmdline_t *cmd,int argc,char *argv[]);

/*  *********************************************************************
    *  Data
    ********************************************************************* */


/*  *********************************************************************
    *  ui_init_bcm1250cpcicmds()
    *  
    *  Add BCM1250CPCI-specific commands to the command table
    *  
    *  Input parameters: 
    *         nothing
    *         
    *  Return value:
    *         0
    ********************************************************************* */


int ui_init_bcm1250cpcicmds(void)
{
    
    cmd_addcmd("setvxwenv",
           ui_bcm1250cpci_setvxwenv,
           NULL,
           "Set up VxWorks environment variables",
           "setvxwenv \n\n"
           "This command enables passing of CFE environment variables to"
           "VxWorks and its processes.",
           "");


    return 0;
}

#ifdef __long64
#define LOCAL_MEM_LOCAL_ADRS    0xFFFFFFFF81000000
#else
#define LOCAL_MEM_LOCAL_ADRS    0x81000000
#endif

typedef struct env_vars
{
    int   offset;
    char *name;
    char *pattern;
} Env_vars;

static Env_vars var_table[] =
{
    { 0x700, "NET_IPADDR", "sbe(0,0)host:/usr/vw/config/sb1250/vxWorks h=90.0.0.3 e=%s u=target" },
    { 0x800, "PARM0", "%s" },
    { 0x880, "PARM1", "%s" },
    { 0x900, "PARM2", "%s" },
    { 0x980, "PARM3", "%s" },
    { 0xA00, "PARM4", "%s" },
    { 0xA80, "PARM5", "%s" },
    { 0xB00, "PARM6", "%s" },
    { 0xB80, "PARM7", "%s" },
    { 0xC00, "PARM8", "%s" },
    { 0xC80, "PARM9", "%s" },
    { 0xD00, "PARMA", "%s" },
    { 0xD80, "PARMB", "%s" },
    { 0xE00, "PARMC", "%s" },
    { 0xE80, "PARMD", "%s" },
    { 0xF00, "PARME", "%s" },
    { 0xF80, "PARMF", "%s" }
};

static int ui_bcm1250cpci_setvxwenv( ui_cmdline_t *cmd, int argc, char *argv[] )
{
    int   var;
    char *vxw_addr;
    char  str[100];
    char  vxw_str[100]; 

    for( var = 0; var < (sizeof(var_table) / sizeof(Env_vars)); var++ )
    {
        str[0] = 0;
        cfe_getenv( var_table[ var ].name, str, sizeof(str) );
        /* xprintf( "%s = %s\n", var_table[ var ].name, str ); */

        sprintf( vxw_str, var_table[ var ].pattern, str );
        /* xprintf( "vxw str = %s\n", vxw_str ); */
        vxw_addr = (char *) (LOCAL_MEM_LOCAL_ADRS + var_table[ var ].offset);
        strcpy( vxw_addr, vxw_str );
    }
    
    return 0;
}
