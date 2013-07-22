/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  P5064-specific commands			File: P5064_COMMANDS.C
    *  
    *  This module contains special command extensions for the
    *  Algorithmics P5064 port of CFE.
    *
    *  NOTE: Some of the routines in this module were borrowed
    *  from PMON.
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

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"

#include "cfe_error.h"
#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"

#include "sbd.h"
#include "rtc.h"

#include "pcivar.h"



#if !defined(__MIPSEB) && !defined(__MIPSEL)
#error "You must define either __MIPSEB or __MIPSEL"
#endif

/* #define POWEROFF */


int ui_init_p6064cmds(void);

#ifdef POWEROFF
static int ui_cmd_poweroff(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif

#if CFG_VGACONSOLE
static int ui_cmd_vgadump(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_vgainit(ui_cmdline_t *cmd,int argc,char *argv[]);
extern int vga_biosinit(void);
extern void vgaraw_dump(char *tail);
#endif

#define INB(x) inb(x)
#define INW(x) inw(x)
#define INL(x) inl(x)

#define OUTB(x,y) outb(x,y)
#define OUTW(x,y) outw(x,y)
#define OUTL(x,y) outl(x,y)


int ui_init_p6064cmds(void)
{

#ifdef POWEROFF
    cmd_addcmd("power off",
	       ui_cmd_poweroff,
	       NULL,
	       "Power off the system.",
	       "power off\n\n"
	       "This command turns off the power for systems that support it.",
	       "");
#endif

#if CFG_VGACONSOLE
    cmd_addcmd("vga init",
	       ui_cmd_vgainit,
	       NULL,
	       "Initialize the VGA adapter.",
	       "vgainit",
	       "");

    cmd_addcmd("vga dumpbios",
	       ui_cmd_vgadump,
	       NULL,
	       "Dump the VGA BIOS to the console",
	       "vga dumpbios",
	       "");
#endif

    return 0;
}


#ifdef POWEROFF
unsigned int apc_bis (int reg, unsigned int val);

unsigned int
apc_bis (int reg, unsigned int val)
{
    unsigned int rtcsa, o, n;

    if (BOARD_REVISION < 'C')
	return ~0;

    OUTB(RTC_ADDR_PORT, RTC_STATUSA);
    rtcsa = INB(RTC_DATA_PORT);
    if ((rtcsa & RTCSA_DVMASK) != RTC_DV2_OSC_ON)
	OUTB(RTC_DATA_PORT, (rtcsa & ~RTCSA_DVMASK) | RTC_DV2_OSC_ON);

    OUTB(RTC_ADDR_PORT, reg);
    o = INB(RTC_DATA_PORT);
    n = o | val;
    if (o != n) {
	OUTB(RTC_DATA_PORT, n);
	}

    /* paranoia - switch back to bank 0 */
    OUTB(RTC_ADDR_PORT, RTC_STATUSA);
    OUTB(RTC_DATA_PORT, (rtcsa & ~RTCSA_DVMASK) | RTC_DV0_OSC_ON);

    return (o);
}

#endif

#if CFG_VGACONSOLE
static int ui_cmd_vgainit(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res;

    res = vga_biosinit();

    xprintf("vgaraw_init returns %d\n",res);
    
    return 0;
}

static int ui_cmd_vgadump(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *x;

    x = cmd_getarg(cmd,0);
    if (!x) x = "";

    vgaraw_dump(x);
    
    return 0;
}
#endif


#ifdef POWEROFF
static int ui_cmd_poweroff(ui_cmdline_t *cmd,int argc,char *argv[])
{
    xprintf("Power off.\n");
    apc_bis(RTC_BANK2_APCR1, APCR1_SOC);
    for (;;) ;
    return 0;
}

#endif
