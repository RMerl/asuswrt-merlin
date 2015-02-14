/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Test commands				File: cfe_dmtest.c
    *  
    *  A temporary sandbox for data mover test routines and commands.
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
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_int.h"
#include "sb1250_dma.h"
/* pass 1 holdover */
#define M_DM_DSCRA_THROTTLE         _SB_MAKEMASK1(43)
#ifdef _SENTOSA_
#include "sentosa.h"
#endif

#include "lib_types.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe.h"
#include "ui_command.h"
#include "cfe_console.h"
#include "cfe_timer.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_irq.h"

#include "lib_hssubr.h"
#include "addrspace.h"

#ifdef __long64         /* Temporary hack */

static long
phys_addr(long va)
{
    long pa;

    if (va - K0BASE >= 0 && va - K0BASE < K0SIZE)
	pa = K0_TO_PHYS(va);
    else if (va - K1BASE >= 0 && va - K1BASE < K1SIZE)
	pa = K1_TO_PHYS(va);
    else
	pa = XKPHYS_TO_PHYS(va);

    return pa;
}

/* to be exported */
void dm_bzero(void *dest, int cnt);
void dm_bsink(void *src, int cnt);
void dm_bcopy(void *src, void *dest, int cnt);
void dm_multibcopy(void *src, void *dest, int cnt, int blksize);

static uint64_t zb_hz;
static uint64_t zb_mhz;

static int ui_cmd_zero(ui_cmdline_t *cmd, int argc, char *argv[]);
static int ui_cmd_sink(ui_cmdline_t *cmd, int argc, char *argv[]);
static int ui_cmd_cpy(ui_cmdline_t *cmd, int argc, char *argv[]);
static int ui_cmd_mcpy(ui_cmdline_t *cmd, int argc, char *argv[]);

/* demo stuff */
static int ui_cmd_testdm(ui_cmdline_t *cmd, int argc, char *argv[]);
static int ui_cmd_talkdm(ui_cmdline_t *cmd, int argc, char *argv[]);

int ui_init_testcmds2(void);
int ui_init_testcmds2(void)
{
    zb_hz = cfe_cpu_speed/2;
    zb_mhz = (zb_hz + 500000)/1000000;

    /* demo commands */
    cmd_addcmd("test dm",
	       ui_cmd_testdm,
	       NULL,
	       "Loop copying memory using the data mover",
	       "test dm [-v|-p] [-u] [-t] [-off] [dest src len]\n\n"
	       "This command continuously copies from source to destination\n "
               " using the data mover and displays MB/s (times 10)",
	       "-p;Address is a physical address|"
	       "-v;Address is a (kernel) virtual address|"
	       "-u;Uncachable source and destination|"
	       "-t;Throttle DMA|"
	       "-off;Stop test");
    cmd_addcmd("talk",
	       ui_cmd_talkdm,
	       NULL,
	       "Text echo",
	       "talk [-l]\n\n"
	       "This command sends to the text demon using the\n "
               " data mover and mailbox variables",
	       "-l;Use local addresses");

    /* timing/utility commands */
    cmd_addcmd("sink",
	       ui_cmd_sink,
	       NULL,
	       "Read memory using the data mover",
	       "sink [-v|-p] [-u|-l2] [addr [len]]\n\n"
	       "This command reads a region of memory using the data\n"
               "mover and discards the result.",
	       "-p;Address is a physical address|"
	       "-v;Address is a (kernel) virtual address|"
	       "-u;Uncachable source|"
	       "-l2;L2 allocate at source");
    cmd_addcmd("zero",
	       ui_cmd_zero,
	       NULL,
	       "Zero memory using the data mover",
	       "zero [-v|-p] [-u|-l2] [addr [len]]\n\n"
	       "This command clears a region of memory to zero using the\n"
               "data mover.",
	       "-p;Address is a physical address|"
	       "-v;Address is a (kernel) virtual address|"
	       "-u;Uncachable destination|"
	       "-l2;L2 allocate at destination");
    cmd_addcmd("mcpy",
	       ui_cmd_mcpy,
	       NULL,
	       "Copy memory using the data mover with multiple descriptors",
	       "cpy [-v|-p] [-u|-l2] dest src [[len] blksize]\n\n"
	       "This command copies from source to destination using the\n"
	       "data mover with blksize bytes per descriptor.",
	       "-p;Address is a physical address|"
	       "-v;Address is a (kernel) virtual address|"
	       "-u;Uncachable source and destination|"
	       "-l2;L2 allocate at destination");
    cmd_addcmd("cpy",
	       ui_cmd_cpy,
	       NULL,
	       "Copy memory using the data mover",
	       "cpy [-v|-p] [-u|-l2] dest src [len]\n\n"
	       "This command copies from source to destination using the\n"
	       "data mover.",
	       "-p;Address is a physical address|"
	       "-v;Address is a (kernel) virtual address|"
	       "-u;Uncachable source and destination|"
	       "-l2;L2 allocate at destination");
    return 0;
}


#define _SBDMA_POLL_

#define dm_nextbuf(d,f) ((((d)->f+1) == (d)->dm_dscrring_end) ? \
			  (d)->dm_dscrring : (d)->f+1)

/* The following macro won't work with 64-bit virtual addresses. */
#define SBDMA_VTOP(x)	((uint64_t) K1_TO_PHYS((uint64_t)((unsigned long)(x))))


/* abbreviations for generating descriptors */

typedef uint64_t flags_t;

#define SRCUNC		M_DM_DSCRA_UN_SRC		/* Source uncached */
#define SRCINC		V_DM_DSCRA_DIR_SRC_INCR		/* inc src ptr */
#define SRCDEC		V_DM_DSCRA_DIR_SRC_DECR		/* dec src ptr */
#define SRCCON		V_DM_DSCRA_DIR_SRC_CONST	/* src ptr constant */

#define DSTUNC 		M_DM_DSCRA_UN_DEST		/* Dest uncached */
#define DSTINC		V_DM_DSCRA_DIR_DEST_INCR	/* inc dest ptr */
#define DSTDEC		V_DM_DSCRA_DIR_DEST_DECR	/* dec dest ptr */
#define DSTCON		V_DM_DSCRA_DIR_DEST_CONST	/* dest ptr constant */

#define ZEROMEM		M_DM_DSCRA_ZERO_MEM		/* just zero dest */
#define PREFETCH	M_DM_DSCRA_PREFETCH		/* don't write dest */
#define L2CDEST		M_DM_DSCRA_L2C_DEST		/* dest l2 cacheable */
#define L2CSRC		M_DM_DSCRA_L2C_SRC		/* src l2 cacheable */

/* NB: THROTTLE is defined only for pass 1 parts. */
#define THROTTLE	M_DM_DSCRA_THROTTLE		/* throttle DMA */

#ifdef _SBDMA_INTERRUPTS_
#define INTER		M_DM_DSCRA_INTERRUPT		/* int on dscr done */
#else
#define INTER		0
#endif


typedef uint32_t sbport_t;
typedef uint64_t sbphysaddr_t;

typedef struct sbdmadscr_s {
    uint64_t  dscr_a;
    uint64_t  dscr_b;
} sbdmadscr_t;

typedef struct sbgendma_s {
    int ch;				/* channel number */

    void        *dm_dscrmem;            /* Memory for descriptor list */

    sbport_t     dm_dscrbase;		/* Descriptor base address */
    sbport_t     dm_dscrcnt;    	/* Descriptor count register */
    sbport_t     dm_curdscr;		/* current descriptor address */

    sbdmadscr_t  *dm_dscrring;		/* base of descriptor table */
    sbdmadscr_t  *dm_dscrring_end;	/* end of descriptor table */

    sbphysaddr_t  dm_dscrring_phys;	/* and also the phys addr */
    sbdmadscr_t  *dm_addptr;		/* next dscr for sw to add */
    sbdmadscr_t  *dm_remptr;		/* next dscr for sw to remove */

    /* number of descriptors we have not told the hardware about yet. */
    int dm_newcnt;

    int dm_busy;
} sbgendma_t;


#define DM_READCSR(x) SBREADCSR(x)
#define DM_WRITECSR(x,y) SBWRITECSR(x,y)

static sbgendma_t sbgendma_ctx[DM_NUM_CHANNELS];

/* Channel allocation:
    0   utility commands that use the data mover (foreground)
    1   data mover test loop (background)
    2   talk command (background)
    3   data mover multicopy (foreground)
*/

#define SBDMA_CMD_CH   0
#define SBDMA_TEST_CH  1
#define SBDMA_TALK_CH  2
#define SBDMA_MCMD_CH  3


static int sbgendma_initialized = 0;


/* For Pass 1, dedicate an SCD peformance counter to use as a counter
   of ZBbus cycles. */
#include "sb1250_scd.h"
#define ZCTR_MODULUS  0x10000000000LL

/* The counter is a shared resource that must be reset periodically
   since it doesn't roll over.  Furthermore, there is a pass one bug
   that makes the interrupt unreliable and the final value either all
   ones or all zeros.  We therefore reset the count when it exceeds
   half the modulus.  We also assume that intervals of interest
   are much less than half the modulus and attempt to adjust for
   the reset in zclk_elapsed. */

static void
zclk_init(uint64_t val)
{
    *((volatile uint64_t *) UNCADDR(A_SCD_PERF_CNT_0)) = val;
    *((volatile uint64_t *) UNCADDR(A_SCD_PERF_CNT_CFG)) =
        V_SPC_CFG_SRC0(1) | M_SPC_CFG_ENABLE;
}

static uint64_t
zclk_get(void)
{
    uint64_t ticks;

    ticks = *((volatile uint64_t *) UNCADDR(A_SCD_PERF_CNT_0));
    if (ticks == 0 || ticks == ZCTR_MODULUS-1) {
	ticks = 0;
	zclk_init(ticks);
	}
    else if (ticks >= ZCTR_MODULUS/2) {
	ticks -= ZCTR_MODULUS/2;
	zclk_init(ticks);  /* Ignore the fudge and lose a few ticks */
	}
    return ticks;
}

static uint64_t
zclk_elapsed(uint64_t stop, uint64_t start)
{
    return ((stop >= start) ? stop : stop + ZCTR_MODULUS/2) - start;
}


/*  *********************************************************************
    *  DM_PROCBUFFERS(d)
    *  
    *  Process "completed" buffers on the specified DMA channel.  
    *  This is normally called within the interrupt service routine.
    *  Note that this isn't really ideal for priority channels, since
    *  it processes all of the packets on a given channel before 
    *  returning. 
    *
    *  Input parameters: 
    *  	   d - DMA channel context
    *  	   
    *  Return value:
    *  	   number of packets processed.
    ********************************************************************* */
static int
dm_procbuffers(sbgendma_t *d)
{
    int curidx;
    int hwidx;
    int count = 0;

    for (;;) {
	/* Figure out where we are (as an index) and where
	   the hardware is (also as an index)
	 
 	   This could be done faster if (for example) the 
	   descriptor table was page-aligned and contiguous in
	   both virtual and physical memory -- you could then
	   just compare the low-order bits of the virtual address
	   (dm_remptr) and the physical address (dm_curdscr CSR) */

	curidx = d->dm_remptr - d->dm_dscrring;
	hwidx = ((DM_READCSR(d->dm_curdscr) & M_DM_CUR_DSCR_DSCR_ADDR) -
		 d->dm_dscrring_phys) / sizeof(sbdmadscr_t);

	/* If they're the same, we've processed all of the descriptors
	   owned by us. */

	if (curidx == hwidx)
	    break;

	/* Otherwise, issue the upcall, or do whatever we want to do
	   to indicate to the client program that the operation is
	   complete (not used here). */

	count++;

	/*  .. and advance to the next buffer. */

	d->dm_remptr = dm_nextbuf(d, dm_remptr);
        }

    return count;
}


/*  *********************************************************************
    *  DM_ADDDESCR(d,newdsc,n)
    *  
    *  Add a descriptor to the specified DMA channel.
    *  
    *  Input parameters: 
    *  	   d - DMA channel descriptor
    *  	   newdsc - pointer to descriptor
    *      n - number of descriptors to add
    *  	   
    *  Return value:
    *  	   number of descriptors actually added
    ********************************************************************* */
static int
dm_adddescr(sbgendma_t *d, sbdmadscr_t *newdsc, int n)
{
    sbdmadscr_t *dsc;
    sbdmadscr_t *nextdsc;
    int cnt = 0;

    while (n > 0) {

	dsc = d->dm_addptr;
	nextdsc = dm_nextbuf(d, dm_addptr);

	/* Test whether the ring is full (next == remptr) */

	if (nextdsc == d->dm_remptr) {
	    break;
	    }

	dsc->dscr_a = newdsc->dscr_a;
	dsc->dscr_b = newdsc->dscr_b;

	d->dm_addptr = nextdsc;

	cnt++;
	n--;
	newdsc++;
	}

    d->dm_newcnt += cnt;

    return cnt;
}


/*  *********************************************************************
    *  DM_START(d)
    *  
    *  Start DMA on the specified channel, queueing all descriptors
    *  that we have added with dm_adddescr()
    *  
    *  Input parameters: 
    *  	   d - DMA context
    *  	   
    *  Return value:
    *  	   start time (zbclks)
    ********************************************************************* */
static uint64_t
dm_start(sbgendma_t *d)
{
    uint64_t now;

    if (d->dm_newcnt) {
	d->dm_busy = 1;
	DM_WRITECSR(d->dm_dscrcnt, d->dm_newcnt);
	}
    now = zclk_get();
    d->dm_newcnt = 0;
    return now;
}


#ifdef _SBDMA_INTERRUPTS_
/*  *********************************************************************
    *  DM_INTERRUPT(ctx)
    *  
    *  "interrupt" routine for a given channel
    *  
    *  Input parameters: 
    *  	   ctx - DMA channel context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void
dm_interrupt(void *ctx)
{
    sbgendma_t *d = (sbgendma_t *)ctx;

    /*  Clear the interrupt (better in procbuffers? */
    (void) DM_READCSR(d->dm_dscrbase); 

    dm_procbuffers(d);

    if (G_DM_CUR_DSCR_DSCR_COUNT(DM_READCSR(d->dm_curdscr)) == 0)
        d->dm_busy = 0;
}


/*  *********************************************************************
    *  DM_ISDONE(d)
    *  
    *  Returns TRUE if all descriptors on the specified channel
    *  are complete.
    *  
    *  Input parameters: 
    *  	   d - DMA context
    *  	   
    *  Return value:
    *  	   1 if all descriptors are processed (none pending in hw)
    *  	   0 if there are still some descriptors in progress
    ********************************************************************* */
static int
dm_isdone(sbgendma_t *d)
{
    cfe_irq_poll(NULL);             /* avoid full bg poll if timing */

    return (d->dm_busy == 0);
}


/*  *********************************************************************
    *  DM_WAIT(d)
    *  
    *  Returns elapsed zb clks when an interrupt becomes pending on
    *  the channel and also processes the interrupt.
    *  CAUTION: spins forever if misused.
    *  
    *  Input parameters: 
    *  	   none
    *  	   
    *  Return value:
    *  	   stop time (zbclks)
    ********************************************************************* */

#define IMR_POINTER(cpu,reg) \
    ((volatile uint64_t *)(PHYS_TO_K1(A_IMR_REGISTER(cpu,reg))))
extern int32_t _getcause(void);		/* return value of CP0 CAUSE */

static uint64_t
dm_wait(sbgendma_t *d)
{
    uint64_t  mask = (1LL << (K_INT_DM_CH_0 + d->ch));
    volatile uint64_t *status = IMR_POINTER(0, R_IMR_INTERRUPT_SOURCE_STATUS);
    uint64_t  stop;

    for (;;) {
	while ((_getcause() & M_CAUSE_IP2) == 0)
	    continue;
	if ((*status & mask) != 0)
	    break;
	cfe_irq_poll(NULL);
	if (d->dm_busy == 0)
	    break;
	}
    stop = zclk_get();

    while (!dm_isdone(d))
	continue;

    return stop;
}
#else  /* !_SBDMA_INTERRUPTS_ */

/*  *********************************************************************
    *  DM_ISDONE(d)
    *  
    *  Returns TRUE if all descriptors on the specified channel
    *  are complete.
    *  
    *  Input parameters: 
    *  	   d - DMA context
    *  	   
    *  Return value:
    *  	   1 if all descriptors are processed (none pending in hw)
    *  	   0 if there are still some descriptors in progress
    ********************************************************************* */
static int
dm_isdone(sbgendma_t *d)
{
#ifdef _SBDMA_TEST_PTRS_
    int curidx, hwidx;
#endif

    /* If polling, now is a good time to process finished descriptors. */

    dm_procbuffers(d);

    /* Test for DMA complete.  There are two ways to do this, to use
       the same technique the interrupt routine uses and compare the
       pointers to the descriptor tables, or we can use the count.
       Both methods are shown below. */

#ifdef _SBDMA_TEST_PTRS_
    curidx = d->dm_addptr - d->dm_dscrring;
    hwidx = ((DM_READCSR(d->dm_curdscr) & M_DM_CUR_DSCR_DSCR_ADDR) -
	     d->dm_dscrring_phys) / sizeof(sbdmadscr_t);
    return (curidx == hwidx);
#else
    return (G_DM_CUR_DSCR_DSCR_COUNT(DM_READCSR(d->dm_curdscr)) == 0);
#endif
}


/*  *********************************************************************
    *  DM_WAIT(d)
    *  
    *  Returns elapsed zb clks when an interrupt becomes pending on
    *  the channel and also processes the interrupt.
    *  CAUTION: spins forever if misused.
    *  
    *  Input parameters: 
    *  	   none
    *  	   
    *  Return value:
    *  	   stop time (zbclks)
    ********************************************************************* */

static uint64_t
dm_wait(sbgendma_t *d)
{
    uint64_t stop;

    while (!dm_isdone(d))
	continue;
    stop = zclk_get();

    d->dm_busy = 0;
    return stop;
}
#endif /* !_SBDMA_INTERRUPTS_ */


/*  *********************************************************************
    *  DM_BUILDDESCR_P(dsc,dest,src,len,flags)
    *  
    *  Builds a descriptor.
    *  
    *  Input parameters: 
    *  	   dsc - pointer to descriptor to build
    *  	   dest - destination address (physical)
    *  	   src - source address (physical)
    *  	   len - number of bytes to transfer
    *  	   flags - flags for transfer
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void
dm_builddescr_p(sbdmadscr_t *dsc, uint64_t dest, uint64_t src,
		int len, flags_t flags)
{
    dsc->dscr_a = dest | flags;
    dsc->dscr_b = src  | V_DM_DSCRB_SRC_LENGTH(len);
}


#define CACHE_ALIGN(x, t)  ((t)(((unsigned long)(x) + 31) & ~31))

/*  *********************************************************************
    *  DM_ALLOCDSCRS(count)
    *  
    *  Allocates dynamic storage for a descriptor ring
    *  
    *  Input parameters: 
    *      cnt - number of descriptors
    *  	   
    *  Return value:
    *  	   pointer to the allocated space (use only cache-aligned slots)
    ********************************************************************* */
static void *
dm_allocdscrs(int cnt)
{
    /* Allocate with enough fudge to cache-align. */
    return KMALLOC(cnt * sizeof(sbdmadscr_t) + 32, 0);
}


/*  *********************************************************************
    *  DM_INITCTX(ch,n,priority)
    *  
    *  Initialize a generic DMA context - this structure describes
    *  the hardware and software state for the device driver.
    *  
    *  Input parameters: 
    *  	   ch - channel number (0..3)
    *      n - desired number of available descriptors
    *          (ring size is n+1 to distinguish full/empty cases)
    *  	   priority - priority for this channel (K_DM_DSCR_BASE_PRIORITY_x)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void
dm_initctx(int ch, int n, int priority)
{
    sbgendma_t *d;
    uint64_t base;

    /* Initialize the fields in the DMA controller's context. */

    d = &sbgendma_ctx[ch];

    d->ch = ch;
    d->dm_dscrbase = (sbport_t) A_DM_REGISTER(ch, R_DM_DSCR_BASE);
    d->dm_dscrcnt  = (sbport_t) A_DM_REGISTER(ch, R_DM_DSCR_COUNT);
    d->dm_curdscr  = (sbport_t) A_DM_REGISTER(ch, R_DM_CUR_DSCR_ADDR);

    /* Initialize the pointers to the DMA descriptor ring. */

    d->dm_dscrmem = dm_allocdscrs(n+1);

    d->dm_dscrring = CACHE_ALIGN(d->dm_dscrmem, sbdmadscr_t *);
    d->dm_dscrring_end = d->dm_dscrring + (n+1);

    d->dm_addptr = d->dm_dscrring;
    d->dm_remptr = d->dm_dscrring;

    d->dm_newcnt = 0;
    d->dm_busy = 0;

    /* Calculate the physical address of the start of the descriptor
       ring.  We need this for determining when the pointers wrap. */

    d->dm_dscrring_phys = (sbphysaddr_t) SBDMA_VTOP(d->dm_dscrring);

    /* Finally, set up the DMA controller. */

    base = d->dm_dscrring_phys | 
	V_DM_DSCR_BASE_PRIORITY(priority) |
	V_DM_DSCR_BASE_RINGSZ(n+1);
    DM_WRITECSR(d->dm_dscrbase, base);
    (void)DM_READCSR(d->dm_dscrbase);     /* push */

    base = d->dm_dscrring_phys | 
	V_DM_DSCR_BASE_PRIORITY(priority) |
	V_DM_DSCR_BASE_RINGSZ(n+1) |
        M_DM_DSCR_BASE_RESET;             /* disable and reset */
    DM_WRITECSR(d->dm_dscrbase, base);
    (void)DM_READCSR(d->dm_dscrbase);     /* push */

    base = d->dm_dscrring_phys | 
	V_DM_DSCR_BASE_PRIORITY(priority) |
	V_DM_DSCR_BASE_RINGSZ(n+1) |
	M_DM_DSCR_BASE_ENABL;             /* enable */
    DM_WRITECSR(d->dm_dscrbase, base);
}


/*  *********************************************************************
    *  DM_UNINITCTX(ch)
    *  
    *  Finalize a generic DMA context - this structure describes
    *  the hardware and software state for the device driver.
    *  
    *  Input parameters: 
    *  	   ch - channel number (0..3)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void
dm_uninitctx(int ch)
{
    sbgendma_t *d;
    uint64_t base;

    /* Finalize the fields in the DMA controller's context. */

    d = &sbgendma_ctx[ch];

    if (d->dm_dscrmem != NULL) {
	KFREE(d->dm_dscrmem);
	}
    d->dm_dscrmem = NULL;

    d->dm_dscrring = NULL;
    d->dm_dscrring_end = NULL;

    d->dm_addptr = NULL;
    d->dm_remptr = NULL;

    d->dm_newcnt = 0;
    d->dm_busy = 0;

    d->dm_dscrring_phys = (sbphysaddr_t) 0;
    base = d->dm_dscrring_phys;
    DM_WRITECSR(d->dm_dscrbase, base);
    (void)DM_READCSR(d->dm_dscrbase);  /* push */

    DM_WRITECSR(d->dm_dscrbase, base | M_DM_DSCR_BASE_RESET);
}


/*  *********************************************************************
    *  DM_STARTONE(ch,dscr)
    *  
    *  Start a single descriptor
    *  
    *  Input parameters: 
    *      ch - DMA channel number.
    *  	   dscr - descriptor to run
    *  	   
    *  Return value:
    *  	   start time (zbclks)
    ********************************************************************* */
static uint64_t
dm_startone(int ch, sbdmadscr_t *dscr)
{
    sbgendma_t *d = &sbgendma_ctx[ch];

    /* Try to put it on the queue.  If this doesn't work, call the
       polling function until we get space.  This can happen if the
       ring is full when we attempt to add a descriptor, which is very
       unlikely in this test program. */

    for (;;) {
	if (dm_adddescr(d, dscr, 1) == 1) {
	    break;
	    }
#ifdef _SBDMA_POLL_
	dm_procbuffers(d);
#endif
	}

    return dm_start(d);
}

/*  *********************************************************************
    *  DM_RUNONE(ch,dscr)
    *  
    *  Run a single descriptor, waiting until it completes before
    *  returning to the calling program.
    *  
    *  Input parameters: 
    *      ch - DMA channel number.
    *  	   dscr - descriptor to run
    *  	   
    *  Return value:
    *  	   elapsed time (zbclks)
    ********************************************************************* */
static uint64_t
dm_runone(int ch, sbdmadscr_t *dscr)
{
    sbgendma_t *d = &sbgendma_ctx[ch];
    uint64_t start, stop;

    start = dm_startone(ch, dscr);
    stop = dm_wait(d);

    return zclk_elapsed(stop, start);
}


/*  *********************************************************************
    *  DM_RESET()
    *  
    *  Reset and initialize global data structures.
    *
    *  Input parameters: 
    *  	   none
    *  	   
    *  Return value:
    *  	   none
    ********************************************************************* */
static void
dm_reset(void)
{
    sbport_t dscrbase_p;
    int ch;

    for (ch = 0; ch < DM_NUM_CHANNELS; ch++) {
        dscrbase_p = (sbport_t) A_DM_REGISTER(ch, R_DM_DSCR_BASE);
        DM_WRITECSR(dscrbase_p, M_DM_DSCR_BASE_RESET);
        }

    for (ch = 0; ch < DM_NUM_CHANNELS; ch++) {
        dscrbase_p = (sbport_t) A_DM_REGISTER(ch, R_DM_DSCR_BASE);
        while (DM_READCSR(dscrbase_p) && M_DM_DSCR_BASE_ACTIVE) {
	    POLL();
	    }

	}
}


/*  *********************************************************************
    *  DM_INIT()
    *  
    *  Initialize global data structures if necessary.
    *
    *  Input parameters: 
    *  	   none
    *  	   
    *  Return value:
    *  	   none
    ********************************************************************* */
static void
dm_init(void)
{
    if (!sbgendma_initialized) {
	dm_reset();

	zclk_init(0);         /* Time origin is arbitrary */

	dm_initctx(SBDMA_CMD_CH, 1, K_DM_DSCR_BASE_PRIORITY_1);
#ifdef _SBDMA_INTERRUPTS_
	cfe_request_irq(K_INT_DM_CH_0 + SBDMA_CMD_CH,
			dm_interrupt, &sbgendma_ctx[SBDMA_CMD_CH], 0, 0);
#endif /* _SBDMA_INTERRUPTS_ */

	sbgendma_initialized = 1;
	}
}


/*  *********************************************************************
    *  DM_CMD_INIT(flush)
    *  
    *  Initialize global data structures, if necessary.
    *
    *  Input parameters: 
    *  	   flush - flush caches first (for timing)
    *  	   
    *  Return value:
    *  	   none
    ********************************************************************* */
static void
dm_cmd_init(int flush)
{
    dm_init();

    if (flush)
	;
}


/*  *********************************************************************
    *  DM_BCOPY_P(src,dest,cnt,flags)
    *  
    *  This is an example implementation of "bcopy" that uses
    *  the data mover.
    *  
    *  Input parameters: 
    *      ch - DMA channel number.
    *  	   src - source physical address
    *  	   dest - destination physical address
    *  	   cnt - count of bytes to copy
    *      flags - option flags
    *  	   
    *  Return value:
    *  	   elapsed time (zbclks)
    ********************************************************************* */
static uint64_t
dm_bcopy_p(int ch, long src, long dest, int cnt, flags_t flags)
{
    sbdmadscr_t dscr;

    if ((unsigned long)src < (unsigned long)dest
	&& (unsigned long)(src + cnt) > (unsigned long)dest) {
        flags |= SRCDEC|DSTDEC;
	dest += cnt - 1;
	src += cnt - 1;
	}
    else {
        flags |= SRCINC|DSTINC;
	}

    dm_builddescr_p(&dscr, dest, src, cnt, flags|INTER);
    return dm_runone(ch, &dscr);
}


/*  *********************************************************************
    *  DM_BCOPY(src,dest,cnt)
    *  
    *  This is an example implementation of "bcopy" that uses
    *  the data mover.
    *  
    *  Input parameters: 
    *  	   src - source virtual address
    *  	   dest - destination virtual address
    *  	   cnt - count of bytes to copy
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void
dm_bcopy(void *src, void *dest, int cnt)
{
    dm_bcopy_p(SBDMA_CMD_CH, phys_addr((long)src), phys_addr((long)dest), cnt, 0);
}


/*  *********************************************************************
    *  DM_MULTIBCOPY_P(ch,src,dest,cnt,flags)
    *  
    *  Input parameters: 
    *      ch - DMA channel number.
    *  	   src - source physical address
    *  	   dest - destination physical address
    *  	   cnt - count of bytes to copy
    *      step - maximum bytes per descriptor
    *      flags - option flags
    *  	   
    *  Return value:
    *  	   elapsed time (zbclks)
    ********************************************************************* */
static uint64_t
dm_multibcopy_p(int ch, long src, long dest, int cnt, int step, flags_t flags)
{
    sbgendma_t *d = &sbgendma_ctx[ch];
    sbdmadscr_t dscr;
    int dnum;
    uint64_t start, stop;

    dnum = (cnt + (step-1))/step;
    if (dnum == 0) {
	return 0;
	}

    flags |= SRCINC|DSTINC;

    dm_init();
    dm_initctx(ch, dnum, K_DM_DSCR_BASE_PRIORITY_1);
#ifdef _SBDMA_INTERRUPTS_
    cfe_request_irq(K_INT_DM_CH_0 + ch, dm_interrupt, d, 0, 0);
#endif

    while (cnt > 0) {
	int n = (cnt < step ? cnt : step);

	if (cnt <= n)
	    flags |= INTER;
    
	dm_builddescr_p(&dscr, dest, src, n, flags);
	dm_adddescr(d, &dscr, 1); 
	src += n;
	dest += n;
	cnt -= n;
	}

    start = dm_start(d);
    stop = dm_wait(d);

    cfe_free_irq(K_INT_DM_CH_0 + ch, 0);
    dm_uninitctx(ch);

    return zclk_elapsed(stop, start);
}

/*  *********************************************************************
    *  DM_MULTIBCOPY(src,dest,cnt)
    *  
    *  This is a variant of BCOPY that queues up several descriptors to
    *  do the work.
    *  
    *  Input parameters: 
    *  	   src - source virtual address
    *  	   dest - destination virtual address
    *  	   len - count of bytes to copy
    *      blksize - maximum bytes per descriptor
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void
dm_multibcopy(void *src, void *dest, int len, int blksize)
{
    dm_multibcopy_p(SBDMA_CMD_CH, phys_addr((long) src), phys_addr((long) dest),
		    len, blksize, 0);
}


/*  *********************************************************************
    *  DM_BZERO_P(dest,cnt,flags)
    *  
    *  Input parameters: 
    *      ch - DMA channel number.
    *  	   dest - physical address
    *  	   cnt - count of bytes to zero
    *      flags - option flags
    *  	   
    *  Return value:
    *  	   elapsed time (zbclks)
    ********************************************************************* */
static uint64_t
dm_bzero_p(int ch, long dest, int cnt, flags_t flags)
{
    sbdmadscr_t dscr;

    flags |= SRCCON|ZEROMEM|DSTINC;
    dm_builddescr_p(&dscr, dest, 0, cnt, flags|INTER);
    return dm_runone(ch, &dscr);
}

/*  *********************************************************************
    *  DM_BZERO(dest,cnt)
    *  
    *  This is an example implementation of "bzero" that uses
    *  the data mover.
    *  
    *  Input parameters: 
    *  	   dest - destination virtual address
    *  	   cnt - count of bytes to zero
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void
dm_bzero(void *dest, int cnt)
{
    dm_bzero_p(SBDMA_CMD_CH, phys_addr((long) dest), cnt, 0);
}


/*  *********************************************************************
    *  DM_BSINK_P(ch,src,cnt,flags)
    *  
    *  Input parameters: 
    *      ch - DMA channel number.
    *  	   src - physical address
    *  	   cnt - count of bytes to read
    *      flags - option flags
    *  	   
    *  Return value:
    *  	   elapsed time (zbclks)
    ********************************************************************* */
static uint64_t
dm_bsink_p(int ch, long src, int cnt, flags_t flags)
{
    sbdmadscr_t dscr;

    flags |= SRCINC|PREFETCH|DSTCON;
    dm_builddescr_p(&dscr, 0, src, cnt, flags|INTER);
    return dm_runone(ch, &dscr);
}

/*  *********************************************************************
    *  DM_BSINK(src,cnt)
    *  
    *  Input parameters: 
    *  	   src - physical address
    *  	   cnt - count of bytes to read
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void
dm_bsink(void *src, int cnt)
{
    dm_bsink_p(SBDMA_CMD_CH, phys_addr((long) src), cnt, 0);
}


/* Address parsing adapted from ui_examcmds. */

#define ATYPE_TYPE_NONE 0
#define ATYPE_TYPE_PHYS 1
#define ATYPE_TYPE_VIRT 2
#define ATYPE_TYPE_MASK (ATYPE_TYPE_PHYS | ATYPE_TYPE_VIRT)

static int
getaddrarg(ui_cmdline_t *cmd, int arg, long *addr, int *atype)
{
    int res;

    res = -1;

    if (atype) {
	*atype &= ~ATYPE_TYPE_MASK;
	*atype |= ATYPE_TYPE_NONE;

	if (cmd_sw_isset(cmd, "-p")) {
	    *atype |= ATYPE_TYPE_PHYS;
	    }
	else if (cmd_sw_isset(cmd, "-v")) {
	    *atype |= ATYPE_TYPE_VIRT;
	    }
	else  {
	    *atype |= ATYPE_TYPE_VIRT;   /* best default? */
	    }
	}

    if (addr) {
	char *x = cmd_getarg(cmd, arg);

	*addr = 0;
	if (x) {
	    /* Following gdb, we Make 64-bit addresses expressed as
	       8-digit numbers sign extend automagically.  Saves
	       typing, but is very gross. */
	    int longaddr;
	    longaddr = strlen(x);
	    if (memcmp(x, "0x", 2) == 0) longaddr -= 2;

	    *addr = (longaddr > 8) ? (long) xtoq(x) : (long) xtoi(x);
	    res = 0;
	    }
	}

    return res;
}

static int
getlenarg(ui_cmdline_t *cmd, int arg, size_t *len)
{
    int res;

    res = -1;

    if (len) {
	char *x = cmd_getarg(cmd, arg);

	*len = 0;
	if (x) {
	    *len = (size_t) xtoi(x);
	    res = 0;
	    }
	}
    return res;
}

static void
show_rate(size_t len, uint64_t elapsed)
{
    uint64_t rate;
    char digits[4+1];

    if (elapsed == 0) elapsed = 1;   /* zbclk always 0 in the simulator */

    rate = ((uint64_t)len * zb_mhz * 10L)/elapsed;
    sprintf(digits, "%04d", rate);
    cfe_ledstr(digits);
    printf("%3d.%1d MB/s\n", rate/10L, rate%10L);
}


static int
ui_cmd_zero(ui_cmdline_t *cmd, int argc, char *argv[])
{
    long dest;
    int  dtype;
    size_t len;
    flags_t flags = 0;
    uint64_t elapsed;

    getaddrarg(cmd, 0, &dest, &dtype);
    getlenarg(cmd, 1, &len);
    if (cmd_sw_isset(cmd, "-u"))
        flags |= DSTUNC;
    else if (cmd_sw_isset(cmd, "-l2"))
	flags |= L2CDEST;      

    if ((dtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	dest = phys_addr(dest);

    if (len > 0x100000) len = 0x100000;

    dm_cmd_init((flags & DSTUNC) != DSTUNC);

    elapsed = dm_bzero_p(SBDMA_CMD_CH, dest, len, flags);

    show_rate(len, elapsed);
    return 0;
}


static int
ui_cmd_sink(ui_cmdline_t *cmd, int argc, char *argv[])
{
    long src;
    int  stype;
    size_t len;
    flags_t flags = 0;
    uint64_t elapsed;

    getaddrarg(cmd, 0, &src, &stype);
    getlenarg(cmd, 1, &len);
    if (cmd_sw_isset(cmd, "-u"))
        flags |= SRCUNC;
    else if (cmd_sw_isset(cmd, "-l2"))
	flags |= L2CSRC;      

    if ((stype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	src = phys_addr(src);

    if (len > 0x100000) len = 0x100000;

    dm_cmd_init((flags & SRCUNC) != SRCUNC);

    elapsed = dm_bsink_p(SBDMA_CMD_CH, src, len, flags);

    show_rate(len, elapsed);
    return 0;
}


static int
ui_cmd_cpy(ui_cmdline_t *cmd, int argc, char *argv[])
{
    long src, dest;
    int  stype, dtype;
    size_t len;
    flags_t flags = 0;
    uint64_t elapsed;

    getaddrarg(cmd, 0, &dest, &dtype);
    getaddrarg(cmd, 1, &src, &stype);
    getlenarg(cmd, 2, &len);
    if (cmd_sw_isset(cmd, "-u"))
        flags |= SRCUNC|DSTUNC;
    else if (cmd_sw_isset(cmd, "-l2"))
	flags |= L2CDEST;      

    if ((dtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	dest = phys_addr(dest);
    if ((stype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	src = phys_addr(src);

    if (len > 0x100000) len = 0x100000;

    dm_cmd_init((flags & (SRCUNC|DSTUNC)) != (SRCUNC|DSTUNC));

    elapsed = dm_bcopy_p(SBDMA_CMD_CH, src, dest, len, flags);

    show_rate(len, elapsed);
    return 0;
}

static int
ui_cmd_mcpy(ui_cmdline_t *cmd, int argc, char *argv[])
{
    long src, dest;
    int  stype, dtype;
    size_t len, blksize;
    flags_t flags = 0;
    uint64_t elapsed;

    getaddrarg(cmd, 0, &dest, &dtype);
    getaddrarg(cmd, 1, &src, &stype);
    getlenarg(cmd, 2, &len);
    getlenarg(cmd, 3, &blksize);
    if (cmd_sw_isset(cmd, "-u"))
        flags |= SRCUNC|DSTUNC;
    else if (cmd_sw_isset(cmd, "-l2"))
	flags |= L2CDEST;      

    if ((dtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	dest = phys_addr(dest);
    if ((stype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	src = phys_addr(src);

    dm_cmd_init((flags & (SRCUNC|DSTUNC)) != (SRCUNC|DSTUNC));

    if (len <= blksize)
	elapsed = dm_bcopy_p(SBDMA_CMD_CH, src, dest, len, flags);
    else
	elapsed = dm_multibcopy_p(SBDMA_MCMD_CH, src, dest, len, blksize, flags);

    show_rate(len, elapsed);
    return 0;
}


/*  *********************************************************************
    *  ui_cmd_testdm()
    *  
    *  Test program.
    *  
    *  Input parameters: 
    *  	   argc,argv
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

#ifdef _SBDMA_INTERRUPTS_

#define N_SAMPLES  8

typedef struct sbgendma_args_s {
    int ch;
    int enabled;
    long src, dest;
    size_t len;
    flags_t flags;
    uint64_t start;
    uint64_t last;
    uint64_t delta[N_SAMPLES];
    int  index;
    int toggle;
    uint64_t count;
} sbgendma_args_t;

static void
dm_restart(sbgendma_args_t *args)
{
    sbdmadscr_t dscr;

    dm_builddescr_p(&dscr, args->dest, args->src, args->len, args->flags);

    args->start = dm_startone(args->ch, &dscr);
}

static void
dm_interrupt_loop(void *ctx)
{
    sbgendma_args_t *args = (sbgendma_args_t *)ctx;
    sbgendma_t *d = &sbgendma_ctx[args->ch];

    /*  Clear the interrupt (better in procbuffers? */
    (void) DM_READCSR(d->dm_dscrbase); 

    dm_procbuffers(d);

    if (G_DM_CUR_DSCR_DSCR_COUNT(DM_READCSR(d->dm_curdscr)) == 0) {
	uint64_t start, now;
	char digits[4+1];

	now = zclk_get();
	start = args->start;

        d->dm_busy = 0;

	if (args->len == 0) {
	    args->enabled = 0;
	    cfe_free_irq(K_INT_DM_CH_0 + args->ch, 0);
	    dm_uninitctx(args->ch);
	    cfe_ledstr("CFE ");
	    }
	else {
	    dm_restart(args);

	    args->delta[args->index] = zclk_elapsed(now, start);
	    args->index = (args->index + 1) % N_SAMPLES;

	    if (zclk_elapsed(now, args->last) > zb_hz/4) {
	        int  i;
	        uint64_t avg, rate;

		args->last = now;
		avg = 0;
		for (i = 0; i < N_SAMPLES; i++)
		    avg += args->delta[i];
		avg /= N_SAMPLES;
		rate = ((uint64_t)(args->len) * zb_mhz * 10L)/avg;
		sprintf(digits, "%04d", rate);
#ifdef _SENTOSA_
#define WRITECSR(csr,val) *((volatile uint64_t *) (csr)) = (val)
		args->toggle ^= 1;
		if (args->toggle)
		    WRITECSR(PHYS_TO_K1(A_GPIO_PIN_SET), M_GPIO_DEBUG_LED);
		else
		    WRITECSR(PHYS_TO_K1(A_GPIO_PIN_CLR), M_GPIO_DEBUG_LED);
		args->count += 1;
		if (args->count % (5*60*4) == 0)
		    xprintf("%ld\n", args->count/(60*4));
#undef WRITECSR
#else
		cfe_ledstr(digits);
#endif
		}
	    }
	}
}

static sbgendma_args_t *test_args[DM_NUM_CHANNELS] = {
    NULL, NULL, NULL, NULL
};

static int
ui_cmd_testdm(ui_cmdline_t *cmd, int argc, char *argv[])
{
    long src, dest;
    int  stype, dtype;
    size_t len;
    flags_t flags = 0;
    sbgendma_args_t *args;
    int ch = SBDMA_TEST_CH;             /* always single channel for now */
    int i;

    if (argc == 0) {
        /* OK values for demos */
        dest = 0xE000120000LL;
	src  = 0x0000020000LL;
	len  = 0x100000LL;
	}
    else {
	getaddrarg(cmd, 0, &dest, &dtype);
	getaddrarg(cmd, 1, &src, &stype);
	getlenarg(cmd, 2, &len);
	if (cmd_sw_isset(cmd, "-u"))
	    flags |= SRCUNC|DSTUNC;
	if (cmd_sw_isset(cmd, "-t"))
	    flags |= THROTTLE;

	if ((dtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	    dest = phys_addr(dest);
	if ((stype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	    src = phys_addr(src);
	}

    if (cmd_sw_isset(cmd, "-off"))
	len = 0;

    if ((unsigned long)src < (unsigned long)dest
	&& (unsigned long)(src + len) > (unsigned long)dest) {
        flags |= SRCDEC|DSTDEC;
	dest += len - 1;
	src += len - 1;
	}
    else {
        flags |= SRCINC|DSTINC;
	}

    args = test_args[ch];
    if (args == NULL) {
	args = (sbgendma_args_t *)KMALLOC(sizeof(sbgendma_args_t), 0);
	if (args == NULL)
	  return -1;
	args->ch = ch;
	args->enabled = 0;
	args->len = 0;
	test_args[ch] = args;
	}

    args->src = src;
    args->dest = dest;
    args->len = len;
    args->flags = flags|INTER;
    args->index = 0;
    for (i = 0; i < N_SAMPLES; i++)
	args->delta[i] = 0;
    args->toggle = 0;
    args->count = 0;

    dm_init();

    if (!args->enabled && len != 0) {
	dm_initctx(args->ch, 1, K_DM_DSCR_BASE_PRIORITY_1);      
	cfe_request_irq(K_INT_DM_CH_0 + args->ch,
			dm_interrupt_loop, args, 0, 0);

        args->enabled = 1;
	args->last = zclk_get();
	dm_restart(args);
	}

    return 0;
}
#else  /* !_SBDMA_INTERRUPTS_ */

static int
ui_cmd_testdm(ui_cmdline_t *cmd, int argc, char *argv[])
{
    long src, dest;
    int  stype, dtype;
    size_t len;
    flags_t flags = 0;
    uint64_t now, elapsed;
    uint64_t rate;
    char digits[4+1];
    uint64_t last;

    getaddrarg(cmd, 0, &dest, &dtype);
    getaddrarg(cmd, 1, &src, &stype);
    getlenarg(cmd, 2, &len);
    if (cmd_sw_isset(cmd, "-u"))
        flags |= SRCUNC|DSTUNC;

    if ((dtype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	dest = phys_addr(dest);
    if ((stype & ATYPE_TYPE_MASK) == ATYPE_TYPE_VIRT)
	src = phys_addr(src);

    last = zclk_get();

    printf("Press ENTER to stop timing\n");

    dm_cmd_init((flags & (SRCUNC|DSTUNC)) != (SRCUNC|DSTUNC));
    dm_initctx(SBDMA_TEST_CH, 1, K_DM_DSCR_BASE_PRIORITY_1);      

    do {    
	elapsed = dm_bcopy_p(SBDMA_TEST_CH, src, dest, len, flags);
	now = zclk_get();

	if (zclk_elapsed(now, last) > zb_hz/4) {
	    last = now;
	    rate = ((uint64_t)len * zb_mhz * 10L)/elapsed;
	    sprintf(digits, "%04d", rate);
	    cfe_ledstr(digits);
	    }

	} while (!console_status());

    dm_uninitctx(SBDMA_TEST_CH);
    return 0;
}
#endif /* !_SBDMA_INTERRUPTS_ */


/* demo stuff */

/* The communication area */

#define SBDMA_REMOTEP(x) ((uint64_t)(x) | 0xe000000000LL)

/* Using physical address 0x10000 as the mailbox base is arbitrary but
   compatible with CFE. */
#define MBOX_PADDR   0x10000
#define BUFFER_PADDR (MBOX_PADDR + sizeof(int))

#define BUFFER_SIZE  200

static int listener = 0;

static void
cfe_recvdm(void *arg)
{
    /* always use the local values */
    volatile int  *mailbox = (volatile int *) PHYS_TO_K1(MBOX_PADDR);
    volatile char *rx_buffer = (volatile char *) PHYS_TO_K1(BUFFER_PADDR);

    if (*mailbox) {
        int len;

	for (len = 0; len < BUFFER_SIZE-1 && rx_buffer[len] != '\000'; len++)
	    ;
	rx_buffer[len] = '\000';
	console_write("[", 1);
	console_write((char *)rx_buffer, len);
	console_write("]\r\n", 3);
	*mailbox = 0;
    }
}

static int
ui_cmd_talkdm(ui_cmdline_t *cmd, int argc, char *argv[])
{
    volatile int *mailbox;
    char tx_buffer[BUFFER_SIZE];
    int  tx_len;
    uint64_t src, dst;
    int local = cmd_sw_isset(cmd, "-l");
    static char prompt[] = "Enter text, exit with \"!<ENTER>\"\r\n";

    if (!listener) {
	*((volatile int *) PHYS_TO_K1(MBOX_PADDR)) = 0;
	cfe_bg_add(cfe_recvdm, NULL);
	listener = 1;
	}

    src = K0_TO_PHYS((uint64_t)tx_buffer);
    if (local) {
	mailbox = (volatile int *) PHYS_TO_K1 (MBOX_PADDR);
	dst = BUFFER_PADDR;
	}
    else {
	mailbox = (volatile int *)
	  PHYS_TO_XKSEG_UNCACHED (SBDMA_REMOTEP(MBOX_PADDR));
	dst = SBDMA_REMOTEP(BUFFER_PADDR);
	}

    dm_init();
    dm_initctx(SBDMA_TALK_CH, 1, K_DM_DSCR_BASE_PRIORITY_1);
#ifdef _SBDMA_INTERRUPTS_
    cfe_request_irq(K_INT_DM_CH_0 + SBDMA_TALK_CH,
		    dm_interrupt, &sbgendma_ctx[SBDMA_TALK_CH], 0, 0);
#endif /* _SBDMA_INTERRUPTS_ */

    console_write(prompt, strlen(prompt));

    for (;;) {
        tx_len = console_readline("", tx_buffer, BUFFER_SIZE-1);

	if (tx_len == 1 && tx_buffer[0] == '!') {
	    console_write("bye\r\n", 5);
	    break;
	    }

	tx_buffer[tx_len] = '\000';
        
	while (*mailbox == 1)
	    POLL();

	dm_bcopy_p(SBDMA_TALK_CH, src, dst, tx_len+1, 0);
	*mailbox = 1;

	while (*mailbox == 1)
	    POLL();
	}

#ifdef _SBDMA_INTERRUPTS_
    cfe_free_irq(K_INT_DM_CH_0 + SBDMA_TALK_CH, 0);
#endif
    dm_uninitctx(SBDMA_TALK_CH);

    return 0;
}

#else  /* ! __long64 */
int ui_init_testcmds2(void);
int ui_init_testcmds2(void)
{
    return 0;
}
#endif /* __long64 */
