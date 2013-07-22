/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SB1250 PCI Device-mode downloader	File: cfe_device_ldr.c
    *  
    *  This invokes the CFE loader to retrieve an elf binary from the
    *  PCI host using the host0 device.
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
#include "lib_printf.h"

#include "cfe_loader.h"
#include "cfe_timer.h"
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#define K1_PTR64(pa) ((uint64_t *)(PHYS_TO_K1(pa)))

extern cfe_loadargs_t cfe_loadargs;

#define MBOX_ARG_WORD  0
#define MBOX_CMD_WORD  1

int cfe_device_download(int boot, char *options);
int
cfe_device_download(int boot, char *options)
{
    cfe_loadargs_t *la = &cfe_loadargs;
    int res;
    uint64_t *mbox_p = K1_PTR64 (A_IMR_REG(NULL, ISTER(0, R_IMR_MAILBOX_CPU));
    uint64_t *mbox_clr = K1_PTR64 (A_IMR_REG(NULL, ISTER(0, R_IMR_MAILBOX_CLR_CPU));
    uint64_t *mbox_set = K1_PTR64 (A_IMR_REG(NULL, ISTER(0, R_IMR_MAILBOX_SET_CPU));
    uint32_t *cmd_p = ((uint32_t *)mbox_p) + MBOX_CMD_WORD;
    
    xprintf("Loader:elf Filesys:raw Dev:host0");

    /* Wait for the host to supply any host-selected options.  There
       is no timeout here because we don't know the host's timing and,
       on a real device card, probably wouldn't have anything to do
       until downloaded. */

    while ((*cmd_p & 0x3) == 0) {
	POLL();
	}

    /* Fill out the loadargs */

    la->la_flags = LOADFLG_NOISY;

    la->la_device = "host0";
    la->la_filesys = "raw";
    la->la_filename = NULL;

    if ((*cmd_p & 0x2) != 0) la->la_flags |= LOADFLG_COMPRESSED;
    la->la_options = options;

    xprintf(" Options:%s\n", la->la_options);

    res = cfe_boot("elf", la);

    /* First supply the result code */
    *((uint32_t *)mbox_clr + MBOX_ARG_WORD) = 0xffffffff;
    *((uint32_t *)mbox_set + MBOX_ARG_WORD) = (uint32_t)res;
    
    /* Then the ack (0x0) */
    *((uint32_t *)mbox_clr + MBOX_CMD_WORD) = 0xffffff;

    if (res < 0) {
	xprintf("Could not download from %s:\n", la->la_device);
	return res;
	}

    if (boot && la->la_entrypt != 0) {
	cfe_go(la);
	/* We won't actually return to here */
	}

    return res;
}
