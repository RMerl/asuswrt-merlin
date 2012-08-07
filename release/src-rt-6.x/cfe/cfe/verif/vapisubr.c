/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Verification Test APIs			File: vapisubr.c
    *
    *  This module contains special low-level routines for use
    *  by verification programs.  The routines here are the "C"
    *  routines for higher-level functions.
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
#include "bsp_config.h"
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_console.h"

#if CFG_VAPI

void vapi_doputs(char *str);
void vapi_dodumpregs(uint64_t *gprs);

const static char * const gpregnames[] = {
    "$0/zero","$1/AT","$2/v0","$3/v1","$4/a0","$5/a1","$6/a2","$7/a3",
    "$8/t0","$9/t1","$10/t2","$11/t3","$12/t4","$13/t5","$14/t6","$15/t7",
    "$16/s0","$17/s1","$18/s2","$19/s3","$20/s4","$21/s5","$22/s6","$23/s7",
    "$24/t8","$25/t9","$26/k0","$27/k1","$28/gp","$29/sp","$30/fp","$31/ra",
    "INX",
    "RAND",
    "TLBLO0",
    "TLBLO1",
    "CTEXT",
    "PGMASK",
    "WIRED",
    "BADVADDR",
    "COUNT",
    "TLBHI",
    "COMPARE",
    "SR",
    "CAUSE",
    "EPC",
    "PRID",
    "CONFIG",
    "LLADDR",
    "WATCHLO",
    "WATCHHI",
    "XCTEXT",
    "ECC",
    "CACHEERR",
    "TAGLO",
    "TAGHI",
    "ERREPC"};

    


void vapi_doputs(char *str)
{
    xprintf("# %s\n",str);
}

void vapi_dodumpregs(uint64_t *gprs)
{
    int cnt = sizeof(gpregnames)/sizeof(char *);
    int idx;

    xprintf("# GPRS:\n");
    for (idx = 0; idx < cnt; idx++) {
	if ((idx & 1) == 0) xprintf("# ");
	xprintf(" %8s=%016llX ",gpregnames[idx],gprs[idx]);
	if ((idx & 1) == 1) xprintf("\n");
	}
    xprintf("\n");

}

#endif /* CFG_VAPI */

/*  *********************************************************************
    *  End
    ********************************************************************* */
