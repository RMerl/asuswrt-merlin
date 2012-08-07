/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  VAPI commands				File: ui_vapi.c
    *  
    *  User interface for the verification API
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
#include "bsp_config.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"

#if CFG_VAPI

#include "vapi.h"

int ui_init_vapicmds(void);

extern void vapitest(void);

extern void vapi_run(int);

extern uint64_t vapi_logstart;
extern uint64_t vapi_logend;
extern uint64_t vapi_logptr;
extern uint64_t vapi_status;

int ui_cmd_vapirun(ui_cmdline_t *cmd,int argc,char *argv[]);
int ui_cmd_vapitest(ui_cmdline_t *cmd,int argc,char *argv[]);
int ui_cmd_vapishow(ui_cmdline_t *cmd,int argc,char *argv[]);
int ui_cmd_vapidump(ui_cmdline_t *cmd,int argc,char *argv[]);
int ui_cmd_vapistatus(ui_cmdline_t *cmd,int argc,char *argv[]);

static char *rectypes[7] = {
    "GPRS ",
    "SOC  ",
    "DATA ",
    "BUF  ",
    "TRC  ",
    "EXIT ",
    "FPRS "
};

int ui_cmd_vapidump(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint64_t *ptr;
    uint64_t *eptr;
    int recnum = 0;

    if (vapi_logptr == 0) {
	xprintf("Diagnostic did not record any log records\n");
	return -1;
	}

    ptr = (uint64_t *) (intptr_t) vapi_logstart;
    eptr = (uint64_t *) (intptr_t) vapi_logptr;

    xprintf("*** VAPI LOG START %s\n",
#ifdef __MIPSEB
	    "big-endian"
#else
	    "little-endian"
#endif
	);

    while (ptr < eptr) {
	xprintf("%6d %016llX %016llX %016llX %016llX\n",
		recnum,
		ptr[0],ptr[1],ptr[2],ptr[3]);
	ptr += 4;
	recnum++;
	}

    xprintf("*** VAPI LOG END\n");

    return 0;
    

}

int ui_cmd_vapishow(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint64_t *ptr;
    uint64_t *eptr;
    uint32_t a,b;
    uint32_t ts,len,fmt;
    unsigned int idx;
    int recnum = 0;

    if (vapi_logptr == 0) {
	xprintf("Diagnostic did not record any log records\n");
	return -1;
	}

    ptr = (uint64_t *) (intptr_t) vapi_logstart;
    eptr = (uint64_t *) (intptr_t) vapi_logptr;

    while (ptr < eptr) {
	a = (ptr[VAPI_IDX_SIGNATURE]) >> 32;
	b = (ptr[VAPI_IDX_SIGNATURE]) & 0xFFFFFFFF;
	if ((a & VAPI_SEAL_MASK) != VAPI_CFESEAL) {
	    xprintf("Incorrect record seal at %08X\n",ptr);
	    break;
	    }

	fmt = (a & VAPI_FMT_MASK);

	xprintf("%5d ID=%08X CPU%d %s RA=%08X ",
		recnum,
		b,
		(a & VAPI_PRID_MASK) >> VAPI_PRNUM_SHIFT,
		rectypes[fmt],
		ptr[VAPI_IDX_RA]);

	ts =  (ptr[VAPI_IDX_SIZE]) >> 32;
	len = ((ptr[VAPI_IDX_SIZE]) & 0xFFFFFFFF);

	xprintf("TS=%08X ",ts);

	switch (fmt) {
	    case VAPI_FMT_GPRS:
		xprintf("Len=%d\n",len);
		for (idx = 0; idx < len; idx += 2) {
		    xprintf("      %016llX  %016llX\n",
			    ptr[VAPI_IDX_DATA+idx],
			    ptr[VAPI_IDX_DATA+idx+1]);
		    }
		break;
	    case VAPI_FMT_SOC:
		xprintf("Len=%d\n",len);
		for (idx = 0; idx < len; idx += 2) {
		    xprintf("      Reg=%016llX  Val=%016llX\n",
			    ptr[VAPI_IDX_DATA+idx],
			    ptr[VAPI_IDX_DATA+idx+1]);
		    }
		break;
	    case VAPI_FMT_DATA:
		xprintf("Data=%016llX\n",ptr[VAPI_IDX_DATA]);
		break;
	    case VAPI_FMT_BUFFER:
		xprintf("Addr=%08X\n",(intptr_t) ptr[VAPI_IDX_DATA]);
		for (idx = 0; idx < len-1; idx += 2) {
		    xprintf("      %016llX  %016llX\n",
			    ptr[VAPI_IDX_DATA+idx+1],
			    ptr[VAPI_IDX_DATA+idx+2]);
		    }
		if (idx != (len-1)) {
		    xprintf("      %016llX\n",
			    ptr[VAPI_IDX_DATA+idx+1]);
		    }
		break;
	    case VAPI_FMT_TRACE:
		xprintf("\n");
		break;
	    case VAPI_FMT_EXIT:
		xprintf("Stat=%016llX\n",ptr[VAPI_IDX_DATA]);
		break;
	    default:
		xprintf("\n");
		break;
	    }

	ptr += 3 + len;
	recnum++;
	}

    return 0;
    
}

int ui_cmd_vapitest(ui_cmdline_t *cmd,int argc,char *argv[])
{

    vapitest();

    xprintf("LogStart=%llX LogEnd=%llX LogPtr=%llX\n",
	    vapi_logstart,vapi_logend,vapi_logptr);

    return 0;
}

int ui_cmd_vapistatus(ui_cmdline_t *cmd,int argc,char *argv[])
{

    xprintf("VAPI Exit Status = <%016llX>\n", vapi_status);
    return 0;
}

int ui_cmd_vapirun(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int mode;		/* 0 = cached, 1 = uncached, 2 = mc mode */

    mode = 0;
    if (cmd_sw_isset(cmd,"-uncached")) mode = 1;
    if (cmd_sw_isset(cmd,"-mc")) mode = 2;

    vapi_run(mode);
    return -1;
}


int ui_init_vapicmds(void)
{
    cmd_addcmd("vapi run",
	       ui_cmd_vapirun,
	       NULL,
	       "Run a program using the VAPI reset vector.",
	       "vapi run\n"
	       "Executes a previously loaded VAPI program by resetting the\n"
	       "CPUs and jumping directly to user code.  The program\n"
	       "must be located at absolute address 0x8002_0000\n",
	       "-uncached;Start execution at 0xA002_0000 (KSEG1)|"
	       "-mc;Start execution at 0xBFD0_0000");

    cmd_addcmd("vapi test",
	       ui_cmd_vapitest,
	       NULL,
	       "Test VAPI interface.",
	       "vapi test\n\n"
	       "Do some basic calls to the VAPI interface, then return to CFE\n\n",
	       "");

    cmd_addcmd("vapi dump",
	       ui_cmd_vapidump,
	       NULL,
	       "Show VAPI log in an easily processed format.",
	       "vapi dump\n\n"
	       "Display the VAPI log in a format that is more easily postprocessed\n"
	       "by external programs.\n\n",
	       "");

    cmd_addcmd("vapi show",
	       ui_cmd_vapishow,
	       NULL,
	       "Show VAPI log.\n",
	       "vapi show\n\n"
	       "Display the VAPI log in a human readable form (sort of)\n\n",
	       "");

    cmd_addcmd("vapi status",
	       ui_cmd_vapistatus,
	       NULL,
	       "Print last VAPI exit status.\n",
	       "vapi status\n\n"
	       "Display the exit status of the last VAPI program that was run\n",
	       "");

    return 0;
}

#endif
