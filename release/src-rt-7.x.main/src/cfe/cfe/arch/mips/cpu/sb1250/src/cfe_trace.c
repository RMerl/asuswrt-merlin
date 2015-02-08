/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Trace commands				File: cfe_trace.c
    *  
    *  A minimal interface for running the SB-1250 trace buffer under CFE 
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
#include "sb1250_regs.h"
#include "sb1250_scd.h"

#include "lib_types.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "ui_command.h"

int ui_init_tracecmds(void);
static int ui_cmd_tracestart(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_tracestop(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_tracefreeze(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_traceread(ui_cmdline_t *cmd,int argc,char *argv[]);


int ui_init_tracecmds(void)
{

    cmd_addcmd("trace start",
	       ui_cmd_tracestart,
	       NULL,
	       "Start trace buffer data collection",
	       "trace start [n [agent]]\n\n"
	       "Start tracing ZBbus transactions, using the n-th canned\n"
               "trigger/filter condition\n"
	       " n = 0: <agent>-mastered transactions\n"
	       " n = 1: CPU0 accesses to LDT dual-hosted region\n",
	       "");
    cmd_addcmd("trace stop",
	       ui_cmd_tracestop,
	       NULL,
	       "Stop trace buffer data collection",
	       "trace stop\n\n"
	       "Stop tracing ZBbus transactions",
	       "");
    cmd_addcmd("trace freeze",
	       ui_cmd_tracefreeze,
	       NULL,
	       "Freeze trace buffer",
	       "trace freeze\n\n"
	       "Freeze the trace buffer",
	       "");
    cmd_addcmd("trace read",
	       ui_cmd_traceread,
	       NULL,
	       "Read trace buffer into memory",
	       "trace read\n\n"
	       "Allocate a block of memory, copy the unprocessed contents\n"
	       "of the trace buffer into it, and generate a command template\n"
	       "for writing it to a file via TFTP.",
	       "");

    return 0;
}

#define TB_SIZE (256*48)    /* 256 entries, 384 bits/entry */
#define ZB_AGENT_CPU0 0
#define ZB_AGENT_CPU1 1
#define ZB_AGENT_IOB0 2
#define ZB_AGENT_IOB1 3
#define ZB_AGENT_SCD  4
#define ZB_AGENT_L2C  6
#define ZB_AGENT_MC   7

static const struct {
    const char *id;
    int         code;
} agent_id[] = {
    { "cpu0", ZB_AGENT_CPU0 },
    { "cpu1", ZB_AGENT_CPU1 },
    { "iob0", ZB_AGENT_IOB0 },
    { "iob1", ZB_AGENT_IOB1 },
    { "scd",  ZB_AGENT_SCD  },
    { "l2c",  ZB_AGENT_L2C  },
    { "mc",   ZB_AGENT_MC   },
    { NULL,   0             }
};

static int
getagentarg (const char *id, int *agent)
{
    int  i;

    for (i = 0; agent_id[i].id != NULL; i++) {
	if (strcmp(id, agent_id[i].id) == 0) {
	    *agent = agent_id[i].code;
	    return 1;
	    }
	}
    return 0;
}

static int
getaddrarg (const char *x, long *addr)
{
    int res;

    res = 0;
    *addr = 0;

    if (x) {
	/* 
	 * Following gdb, we make 64-bit addresses expressed as
	 * 8-digit numbers sign extend automagically.  Saves
	 * typing, but is very gross.
	 */
	int longaddr;

	longaddr = strlen(x);
	if (memcmp(x,"0x",2) == 0) longaddr -= 2;
      
	*addr = (longaddr > 8) ? (long) xtoq(x) : (long) xtoi(x);
	res = 1;
	}

    return res;
}


static int ui_cmd_tracestart(ui_cmdline_t *cmd, int argc, char *argv[])
{
    volatile uint64_t *tb_cfg;

    tb_cfg = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_CFG);

    if (argc != 0) {
        volatile uint64_t *at_down_0, *at_up_0, *at_cfg_0;
        volatile uint64_t *tb_evt_0, *tb_evt_4;
	volatile uint64_t *tb_seq_0, *tb_seq_4;
	int i, n;
	int found;
	int agent;

	/* Place holder: parse new trace spec (select an option for now) */

	n = atoi(cmd_getarg(cmd,0));

	at_down_0 = (volatile uint64_t *) PHYS_TO_K1(A_ADDR_TRAP_DOWN_0);
	at_up_0   = (volatile uint64_t *) PHYS_TO_K1(A_ADDR_TRAP_UP_0);
	at_cfg_0  = (volatile uint64_t *) PHYS_TO_K1(A_ADDR_TRAP_CFG_0);

	tb_evt_0 = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_EVENT_0);
	tb_evt_4 = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_EVENT_4);
	tb_seq_0 = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_SEQUENCE_0);
	tb_seq_4 = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_SEQUENCE_4);
	
	/* Reset everything */
	*tb_cfg = M_SCD_TRACE_CFG_RESET;
	__asm__ __volatile__ ("sync" : : : "memory");

	for (i = 0; i < 4; i++) {
	    tb_evt_0[i] = 0;  tb_evt_4[i] = 0;
	    tb_seq_0[i] = 0;  tb_seq_4[i] = 0;
	}
	__asm__ __volatile__ ("sync" : : : "memory");

	switch (n) {
	case 0:
	    /* Filter on agent request or response */
	    if (argc > 1)
		found = getagentarg(cmd_getarg(cmd,1), &agent);
	    else
		found = 0;
	    if (!found)
	        agent = ZB_AGENT_IOB0;   /* temporary, for backward compat */

	    tb_evt_0[0] = M_SCD_TREVT_REQID_MATCH
	                | (M_SCD_TREVT_READ | M_SCD_TREVT_WRITE)
	       	        | V_SCD_TREVT_REQID(agent);
	    tb_evt_0[1] = M_SCD_TREVT_DATAID_MATCH
	                | V_SCD_TREVT_DATAID(agent);
	
	    tb_seq_0[0] = 0x0fff
                        | M_SCD_TRSEQ_ASAMPLE;
	    tb_seq_0[1] = 0x1fff
                        | M_SCD_TRSEQ_DSAMPLE;
	    break;
	case 1:
	    /* Filter on CPU accesses in LDT mirror region. */
	    (void)getaddrarg;

	    at_down_0[0] = 0xe000000000;
	    at_up_0[0]   = 0xefffffffff;
	    at_cfg_0[0]  = M_ATRAP_CFG_ALL;

	    tb_evt_0[0] = M_SCD_TREVT_REQID_MATCH
	                | V_SCD_TREVT_ADDR_MATCH(0x1) /* addr trap 0 */
	                | (M_SCD_TREVT_READ | M_SCD_TREVT_WRITE)
	                | V_SCD_TREVT_REQID(ZB_AGENT_CPU0);
	    tb_evt_0[1] = M_SCD_TREVT_RESPID_MATCH
	                | V_SCD_TREVT_RESPID(ZB_AGENT_IOB0);
	
	    tb_seq_0[0] = 0x0fff
	                | M_SCD_TRSEQ_ASAMPLE;
	    tb_seq_0[1] = 0x1fff
	                | M_SCD_TRSEQ_DSAMPLE;
	    break;
	default:
	    break;
	}

	__asm__ __volatile__ ("sync" : : : "memory");
printf("trace start: trace 0: %llx 1: %llx\n", tb_evt_0[0], tb_evt_0[1]);
printf("             seq   0: %llx 1: %llx\n", tb_seq_0[0], tb_seq_0[1]);
    }

    *tb_cfg = M_SCD_TRACE_CFG_START;
    __asm__ __volatile__ ("sync" : : : "memory");

printf("trace start: cfg %llx\n", *tb_cfg);
    return 0;
}

static int ui_cmd_tracestop(ui_cmdline_t *cmd, int argc, char *argv[])
{
    volatile uint64_t *tb_cfg;

    tb_cfg = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_CFG);
    *tb_cfg = M_SCD_TRACE_CFG_STOP;
    __asm__ __volatile__ ("sync" : : : "memory");

printf("trace stop: cfg %llx\n", *tb_cfg);
    return 0;
}

static int ui_cmd_tracefreeze(ui_cmdline_t *cmd, int argc, char *argv[])
{
    volatile uint64_t *tb_cfg;

    tb_cfg = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_CFG);
    *tb_cfg = M_SCD_TRACE_CFG_FREEZE;
    __asm__ __volatile__ ("sync" : : : "memory");

printf("trace freeze: cfg %llx\n", *tb_cfg);
    return 0;
}

/* read out trace buffer to dump to a file */

static int ui_cmd_traceread(ui_cmdline_t *cmd, int argc, char *argv[])
{
    uint64_t *p;
    volatile uint64_t *tb_cfg;
    volatile uint64_t *tb_read;
    int  i;

    p = KMALLOC(TB_SIZE, 0);
    if (!p) {
	printf("trace read: buffer allocation failed\n");
	return -1;
	}

    tb_cfg = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_CFG);
    tb_read = (volatile uint64_t *) PHYS_TO_K1(A_SCD_TRACE_READ);

    while ((*tb_cfg & M_SCD_TRACE_CFG_FREEZE) == 0) {
        *tb_cfg = M_SCD_TRACE_CFG_FREEZE;
	__asm__ __volatile__ ("sync" : : : "memory");
	}

    *tb_cfg = M_SCD_TRACE_CFG_START_READ;
    __asm__ __volatile__ ("sync" : : : "memory");

    /* Loop runs backward because bundles are read out in reverse order. */
    for (i = 256*6; i > 0; i -= 6) {
        /* Subscripts decrease to put bundle in order. */
	p[i-1] = *tb_read;    /* t2 hi */
        p[i-2] = *tb_read;    /* t2 lo */
        p[i-3] = *tb_read;    /* t1 hi */
        p[i-4] = *tb_read;    /* t1 lo */
        p[i-5] = *tb_read;    /* t0 hi */
        p[i-6] = *tb_read;    /* t0 lo */
	}

    printf("save hardy:ehs/sample.trace %p 0x%lx\n", p, TB_SIZE);

    return 0;
}
