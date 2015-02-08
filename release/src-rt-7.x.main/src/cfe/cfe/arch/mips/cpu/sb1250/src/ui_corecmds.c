/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Core hacking commands			File: ui_corecmds.c
    *  
    *  Core mutilation enabler
    *  
    *  Author:  Justin Carlson (carlson@broadcom.com)
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
#include "lib_arena.h"

#include "ui_command.h"

#include "cfe.h"





int ui_init_corecmds(void);
extern void _cfe_flushcache(int);
extern void sb1250_l2cache_flush(void);

static int ui_cmd_defeature(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_show_defeature(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_show_tlb(ui_cmdline_t *cmd,int argc,char *argv[]);

/* 
 * This is sneaky and cruel.  Basically we muck with the return
 * address and don't tell the compiler about it.  That way
 * we return happily to the desired segment.  Return
 * bits 31-29 of the address in the top posi
 */
#if CFG_RUNFROMKSEG0
static void segment_bounce(unsigned int bits)
{
	__asm__ __volatile__ (
		".set push             \n"
		".set noat             \n"
		".set noreorder        \n"
		"    lui  $8,0x1FFF    \n"
		"    ori  $8,$8,0xFFFF \n"
		"    move $2, $31      \n"
		"    and  $31, $31, $8  \n"
		"    or   $31, $31, $4 \n"
		".set pop              \n"
		:
		:
		:"$1","$8" /* *NOT* $31!  We don't want the compiler saving it */
		);
}
#endif


int ui_init_corecmds(void)
{
    cmd_addcmd("defeature",
	       ui_cmd_defeature,
	       NULL,
	       "Set the state of the defeature mask",
	       "defeature <hex mask>\n\n"
	       "This command pokes the given value into cp0 register 23 select 2\n"
	       "to defeature functionality.  The mask is not checked for sanity\n"
	       "so use this command carefully!\n",
	       "");

    cmd_addcmd("show defeature",
	       ui_cmd_show_defeature,
	       NULL,
	       "Show the state of the defeature mask",
	       "show defeature\n\n"
	       "Gets the current state of the defeature mask from xp0 reg 23,\n"
	       "select 2 and prints it\n",
	       "");

    cmd_addcmd("show tlb",
	       ui_cmd_show_tlb,
	       NULL,
	       "Show the contents of the TLB",
	       "show tlb [low [high]]\n\n"
	       "Displays all the entries in the TLB\n"
	       "You can specify a range of TLB entries, in decimal",
	       "");

    return 0;
}

static int ui_cmd_defeature(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *value_str;
    unsigned int value;

    value_str  = cmd_getarg(cmd, 0);

    if (value_str == NULL) {
	return ui_showusage(cmd);
        }

    value = lib_xtoi(value_str);
    
#if CFG_RUNFROMKSEG0
    segment_bounce(0xa0000000);
    _cfe_flushcache(0);
    sb1250_l2cache_flush();
#endif

    __asm__ __volatile__ (
	    ".set push ; .set mips64 ;  mtc0 %0, $23, 2 ; .set pop\n"
	    : /* No outputs */
	    : "r" (value)
	    );

#if CFG_RUNFROMKSEG0
//    _cfe_flushcache(0);
//    sb1250_l2cache_flush();
    segment_bounce(0x80000000);
#endif

    return 0;
}

static int ui_cmd_show_defeature(ui_cmdline_t *cmd,int argc,char *argv[])
{
	unsigned int value;

	__asm__ __volatile__ (
		" .set push ; .set mips64 ;  mfc0 %0, $23, 2 ; .set pop\n"
		: "=r" (value));
	xprintf("Defeature mask is currently %08X\n", value);
	return 0;
}

static int ui_cmd_show_tlb(ui_cmdline_t *cmd,int argc,char *argv[])
{
	uint64_t valuehi,valuelo0,valuelo1;
	int idx;
	int low = 0;
	int high = K_NTLBENTRIES;
	char *x;

	if ((x = cmd_getarg(cmd,0))) {
	    low = atoi(x);
	    high = low+1;
	    }
	if ((x = cmd_getarg(cmd,1))) {
	    high = atoi(x)+1;
	    }
	

	for (idx = low ; idx < high; idx++) {

	    __asm__ __volatile__ (
		"     .set push ; .set mips64 \n"
		"     mtc0 %3, $0 \n"
		"     tlbr \n"
		"     dmfc0 %0,$10 \n"
		"     dmfc0 %1,$2  \n"
		"     dmfc0 %2,$3  \n"
		"     .set pop \n"
		: "=r" (valuehi) , "=r" (valuelo0) , "=r" (valuelo1)
		: "r" (idx)
		);

	    printf("Entry %2d  %016llX %016llX %016llX\n",idx,valuehi,valuelo0,valuelo1);
	    }

	return 0;
}
