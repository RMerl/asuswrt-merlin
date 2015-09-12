/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  OHCI device driver			File: ohci.c
    *  
    *  Open Host Controller Interface low-level routines
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


#ifndef _CFE_
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "usbhack.h"
#define CPUCFG_COHERENT_DMA 1	/* hack runs on a PC, PCs are coherent */
#else
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "lib_physio.h"
#include "addrspace.h"
#include "cpu_config.h"		/* for CPUCFG_COHERENT_DMA */
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"
#include "ohci.h"


/*  *********************************************************************
    *  Macros for dealing with hardware
    *
    *  This is all yucky stuff that needs to be made more
    *  processor-independent.  It's mostly here now to help us with
    *  our test harness.
    ********************************************************************* */

#if defined(_CFE_) && defined(__MIPSEB)
#define BSWAP32(x) __swap32(x)
static inline uint32_t __swap32(uint32_t x)
{
    uint32_t y;

    y = ((x & 0xFF) << 24) |
	((x & 0xFF00) << 8) |
	((x & 0xFF0000) >> 8) |
	((x & 0xFF000000) >> 24);

    return y;
}
#else
#define BSWAP32(x) (x)
#endif


#ifndef _CFE_
extern uint32_t vtop(void *ptr);
extern void *ptov(uint32_t x);
#define OHCI_VTOP(ptr) vtop(ptr)
#define OHCI_PTOV(ptr) ptov(ptr)
#define OHCI_WRITECSR(softc,x,y) \
    *((volatile uint32_t *) ((softc)->ohci_regs + ((x)/sizeof(uint32_t)))) = (y)
#define OHCI_READCSR(softc,x) \
    *((volatile uint32_t *) ((softc)->ohci_regs + ((x)/sizeof(uint32_t))))
#else
#define OHCI_VTOP(ptr) ((uint32_t)PHYSADDR((long)(ptr)))

#if CPUCFG_COHERENT_DMA
#define OHCI_PTOV(ptr) ((void *)(KERNADDR(ptr)))
#else
#define OHCI_PTOV(ptr) ((void *)(UNCADDR(ptr)))
#endif

#define OHCI_WRITECSR(softc,x,y) \
    phys_write32(((softc)->ohci_regs + (x)),(y))
#define OHCI_READCSR(softc,x) \
    phys_read32(((softc)->ohci_regs + (x)))
#endif

#if CPUCFG_COHERENT_DMA
#define OHCI_INVAL_RANGE(s,l)
#define OHCI_FLUSH_RANGE(s,l)
#else	 /* not coherent */
#define CFE_CACHE_INVAL_RANGE	32
#define CFE_CACHE_FLUSH_RANGE	64
extern void _cfe_flushcache(int,uint8_t *,uint8_t *);
#define OHCI_INVAL_RANGE(s,l) _cfe_flushcache(CFE_CACHE_INVAL_RANGE,((uint8_t *) (s)),((uint8_t *) (s))+(l))
#define OHCI_FLUSH_RANGE(s,l) _cfe_flushcache(CFE_CACHE_FLUSH_RANGE,((uint8_t *) (s)),((uint8_t *) (s))+(l))
#endif


/*  *********************************************************************
    *  Bit-reverse table - this table consists of the numbers
    *  at its index, listed in reverse.  So, the reverse of 0000 0010
    *  is 0100 0000.
    ********************************************************************* */

const static int ohci_revbits[OHCI_INTTABLE_SIZE] = {
    0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0c, 0x1c,
    0x02, 0x12, 0x0a, 0x1a, 0x06, 0x16, 0x0e, 0x1e,
    0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0d, 0x1d,
    0x03, 0x13, 0x0b, 0x1b, 0x07, 0x17, 0x0f, 0x1f
};


/*  *********************************************************************
    *  Macros to convert from "hardware" endpoint and transfer
    *  descriptors (ohci_ed_t, ohci_td_t) to "software"
    *  data structures (ohci_transfer_t, ohci_endpoint_t).  
    *  
    *  Basically, there are two tables, indexed by the same value
    *  By subtracting the base of one pool from a pointer, we get
    *  the index into the other table.  
    *  
    *  We *could* have included the ed and td in the software
    *  data structures, but placing all the hardware stuff in one
    *  pool will make it easier for hardware that does not handle
    *  coherent DMA, since we can be less careful about what we flush
    *  and what we invalidate.
    ********************************************************************* */

#define ohci_td_from_transfer(softc,transfer) \
     ((softc)->ohci_hwtdpool + ((transfer) - (softc)->ohci_transfer_pool))

#define ohci_transfer_from_td(softc,td) \
     ((softc)->ohci_transfer_pool + ((td) - (softc)->ohci_hwtdpool))

#define ohci_ed_from_endpoint(softc,endpoint) \
     ((softc)->ohci_hwedpool + ((endpoint) - (softc)->ohci_endpoint_pool))

#define ohci_endpoint_from_ed(softc,ed) \
     ((softc)->ohci_endpoint_pool + ((ed) - (softc)->ohci_hwedpool))

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static int ohci_roothub_xfer(usbbus_t *bus,usb_ept_t *uept,usbreq_t *ur);
static void ohci_roothub_statchg(ohci_softc_t *softc);
extern usb_hcdrv_t ohci_driver;

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

int ohcidebug = 0;
void ohci_dumprhstat(uint32_t reg);
void ohci_dumpportstat(int idx,uint32_t reg);
void ohci_dumptd(ohci_td_t *td);
void ohci_dumptdchain(ohci_td_t *td);
void ohci_dumped(ohci_ed_t *ed);
void ohci_dumpedchain(ohci_ed_t *ed);


/*  *********************************************************************
    *  Some debug routines
    ********************************************************************* */

void ohci_dumprhstat(uint32_t reg)
{
    printf("HubStatus: %08X  ",reg);

    if (reg & M_OHCI_RHSTATUS_LPS) printf("LocalPowerStatus ");
    if (reg & M_OHCI_RHSTATUS_OCI) printf("OverCurrent ");
    if (reg & M_OHCI_RHSTATUS_DRWE) printf("DeviceRemoteWakeupEnable ");
    if (reg & M_OHCI_RHSTATUS_LPSC) printf("LocalPowerStatusChange ");
    if (reg & M_OHCI_RHSTATUS_OCIC) printf("OverCurrentIndicatorChange ");
    printf("\n");

}

void ohci_dumpportstat(int idx,uint32_t reg)
{
    printf("Port %d: %08X  ",idx,reg);
    if (reg & M_OHCI_RHPORTSTAT_CCS) printf("Connected ");
    if (reg & M_OHCI_RHPORTSTAT_PES) printf("PortEnabled ");
    if (reg & M_OHCI_RHPORTSTAT_PSS) printf("PortSuspended ");
    if (reg & M_OHCI_RHPORTSTAT_POCI) printf("PortOverCurrent ");
    if (reg & M_OHCI_RHPORTSTAT_PRS) printf("PortReset ");
    if (reg & M_OHCI_RHPORTSTAT_PPS) printf("PortPowered ");
    if (reg & M_OHCI_RHPORTSTAT_LSDA) printf("LowSpeed ");
    if (reg & M_OHCI_RHPORTSTAT_CSC) printf("ConnectStatusChange ");
    if (reg & M_OHCI_RHPORTSTAT_PESC) printf("PortEnableStatusChange ");
    if (reg & M_OHCI_RHPORTSTAT_PSSC) printf("PortSuspendStatusChange ");
    if (reg & M_OHCI_RHPORTSTAT_OCIC) printf("OverCurrentIndicatorChange ");
    if (reg & M_OHCI_RHPORTSTAT_PRSC) printf("PortResetStatusChange ");
    printf("\n");
}

void ohci_dumptd(ohci_td_t *td)
{
    uint32_t ctl;
    static char *pids[4] = {"SETUP","OUT","IN","RSVD"};

    ctl = BSWAP32(td->td_control);

    printf("[%08X] ctl=%08X (DP=%s,DI=%d,T=%d,EC=%d,CC=%d%s) cbp=%08X be=%08X next=%08X\n",
	   OHCI_VTOP(td),
	   ctl,
	   pids[G_OHCI_TD_PID(ctl)],
	   G_OHCI_TD_DI(ctl),
	   G_OHCI_TD_DT(ctl),
	   G_OHCI_TD_EC(ctl),
	   G_OHCI_TD_CC(ctl),
	   (ctl & M_OHCI_TD_SHORTOK) ? ",R" : "",
	   BSWAP32(td->td_cbp),
	   BSWAP32(td->td_be),
	   BSWAP32(td->td_next_td));
}

void ohci_dumptdchain(ohci_td_t *td)
{
    int idx = 0;
    for (;;) {
	printf("%d:[%08X] ctl=%08X cbp=%08X be=%08X next=%08X\n",
	       idx,
	   OHCI_VTOP(td),
	   BSWAP32(td->td_control),
	   BSWAP32(td->td_cbp),
	   BSWAP32(td->td_be),
	   BSWAP32(td->td_next_td));
	if (!td->td_next_td) break;
	td = (ohci_td_t *) OHCI_PTOV(BSWAP32(td->td_next_td));
	idx++;
	}
}

void ohci_dumped(ohci_ed_t *ed)
{
    uint32_t ctl;
    static char *pids[4] = {"FTD","OUT","IN","FTD"};

    ctl = BSWAP32(ed->ed_control),

    printf("[%08X] Ctl=%08X (MPS=%d%s%s%s,EN=%d,FA=%d,D=%s) Tailp=%08X headp=%08X next=%08X %s\n",
	   OHCI_VTOP(ed),
	   ctl,
	   G_OHCI_ED_MPS(ctl),
	   (ctl & M_OHCI_ED_LOWSPEED) ? ",LS" : "",
	   (ctl & M_OHCI_ED_SKIP) ? ",SKIP" : "",
	   (ctl & M_OHCI_ED_ISOCFMT) ? ",ISOC" : "",
	   G_OHCI_ED_EN(ctl),
	   G_OHCI_ED_FA(ctl),
	   pids[G_OHCI_ED_DIR(ctl)],
	   BSWAP32(ed->ed_tailp),
	   BSWAP32(ed->ed_headp),
	   BSWAP32(ed->ed_next_ed),
	   BSWAP32(ed->ed_headp) & M_OHCI_ED_HALT ? "HALT" : "");
    if ((ed->ed_headp & M_OHCI_ED_PTRMASK) == 0) return;
    ohci_dumptdchain(OHCI_PTOV(BSWAP32(ed->ed_headp) & M_OHCI_ED_PTRMASK));
}

void ohci_dumpedchain(ohci_ed_t *ed)
{
    int idx = 0;
    for (;;) {
	printf("---\nED#%d -> ",idx);
	ohci_dumped(ed);
	if (!ed->ed_next_ed) break;
	if (idx > 50) break;
	ed = (ohci_ed_t *) OHCI_PTOV(BSWAP32(ed->ed_next_ed));
	idx++;
	}
}


static void eptstats(ohci_softc_t *softc)
{
    int cnt;
    ohci_endpoint_t *e;
    cnt = 0; 

    e = softc->ohci_endpoint_freelist;
    while (e) { e = e->ep_next; cnt++; }
    printf("%d left, %d inuse\n",cnt,OHCI_EDPOOL_SIZE-cnt);
}

/*  *********************************************************************
    *  _ohci_allocept(softc)
    *  
    *  Allocate an endpoint data structure from the pool, and 
    *  make it ready for use.  The endpoint is NOT attached to 
    *  the hardware at this time.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   
    *  Return value:
    *  	   pointer to endpoint or NULL
    ********************************************************************* */

static ohci_endpoint_t *_ohci_allocept(ohci_softc_t *softc)
{
    ohci_endpoint_t *e;
    ohci_ed_t *ed;

    if (ohcidebug > 2) {
	printf("AllocEpt: ");eptstats(softc);
	}

    e = softc->ohci_endpoint_freelist;

    if (!e) {
	printf("No endpoints left!\n");
	return NULL;
	}

    softc->ohci_endpoint_freelist = e->ep_next;

    ed = ohci_ed_from_endpoint(softc,e);

    ed->ed_control = BSWAP32(M_OHCI_ED_SKIP);
    ed->ed_tailp   = BSWAP32(0);
    ed->ed_headp   = BSWAP32(0);
    ed->ed_next_ed = BSWAP32(0);

    e->ep_phys = OHCI_VTOP(ed);
    e->ep_next = NULL;

    return e;
}

/*  *********************************************************************
    *  _ohci_allocxfer(softc)
    *  
    *  Allocate a transfer descriptor.  It is prepared for use
    *  but not attached to the hardware.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   
    *  Return value:
    *  	   transfer descriptor, or NULL
    ********************************************************************* */

static ohci_transfer_t *_ohci_allocxfer(ohci_softc_t *softc)
{
    ohci_transfer_t *t;
    ohci_td_t *td;

    if (ohcidebug > 2) {
	int cnt;
	cnt = 0; 
	t = softc->ohci_transfer_freelist;
	while (t) { t = t->t_next; cnt++; }
	printf("AllocXfer: %d left, %d inuse\n",cnt,OHCI_TDPOOL_SIZE-cnt);
	}

    t = softc->ohci_transfer_freelist;

    if (!t) {
	printf("No more transfer descriptors!\n");
	return NULL;
	}

    softc->ohci_transfer_freelist = t->t_next;

    td = ohci_td_from_transfer(softc,t);

    td->td_control = BSWAP32(0);
    td->td_cbp     = BSWAP32(0);
    td->td_next_td = BSWAP32(0);
    td->td_be      = BSWAP32(0);

    t->t_ref  = NULL;
    t->t_next = NULL;

    return t;
}

/*  *********************************************************************
    *  _ohci_freeept(softc,e)
    *  
    *  Free an endpoint, returning it to the pool.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   e - endpoint descriptor to return
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _ohci_freeept(ohci_softc_t *softc,ohci_endpoint_t *e)
{
    if (ohcidebug > 2) {
	int cnt;
	ohci_endpoint_t *ee;
	cnt = 0; 
	ee = softc->ohci_endpoint_freelist;
	while (ee) { ee = ee->ep_next; cnt++; }
	printf("FreeEpt[%p]: %d left, %d inuse\n",e,cnt,OHCI_EDPOOL_SIZE-cnt);
	}

    e->ep_next = softc->ohci_endpoint_freelist;
    softc->ohci_endpoint_freelist = e;
}

/*  *********************************************************************
    *  _ohci_freexfer(softc,t)
    *  
    *  Free a transfer descriptor, returning it to the pool.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   t - transfer descriptor to return
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _ohci_freexfer(ohci_softc_t *softc,ohci_transfer_t *t)
{
    t->t_next = softc->ohci_transfer_freelist;
    softc->ohci_transfer_freelist = t;
}

/*  *********************************************************************
    *  _ohci_initpools(softc)
    *  
    *  Allocate and initialize the various pools of things that
    *  we use in the OHCI driver.  We do this by allocating some
    *  big chunks from the heap and carving them up.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int _ohci_initpools(ohci_softc_t *softc)
{
    int idx;

    /*
     * Do the transfer descriptor pool
     */

    softc->ohci_transfer_pool = KMALLOC(OHCI_TDPOOL_SIZE*sizeof(ohci_transfer_t),0);
    softc->ohci_hwtdpool = KMALLOC(OHCI_TDPOOL_SIZE*sizeof(ohci_td_t),OHCI_TD_ALIGN);

    /*
     * In the case of noncoherent DMA, make these uncached addresses.
     * This way all our descriptors will be uncached.  Makes life easier, as we
     * do not need to worry about flushing descriptors, etc.
     */

#if !CPUCFG_COHERENT_DMA
    softc->ohci_hwtdpool = (void *) UNCADDR(PHYSADDR((uint32_t)(softc->ohci_hwtdpool)));
#endif

    if (!softc->ohci_transfer_pool || !softc->ohci_hwtdpool) {
	printf("Could not allocate transfer descriptors\n");
	return -1;
	}

    softc->ohci_transfer_freelist = NULL;

    for (idx = 0; idx < OHCI_TDPOOL_SIZE; idx++) {
	_ohci_freexfer(softc,softc->ohci_transfer_pool+idx);
	}

    /*
     * Do the endpoint descriptor pool
     */
	 
    softc->ohci_endpoint_pool = KMALLOC(OHCI_EDPOOL_SIZE*sizeof(ohci_endpoint_t),0);

    softc->ohci_hwedpool       = KMALLOC(OHCI_EDPOOL_SIZE*sizeof(ohci_ed_t),OHCI_ED_ALIGN);

#if !CPUCFG_COHERENT_DMA
    softc->ohci_hwedpool = (void *) UNCADDR(PHYSADDR((uint32_t)(softc->ohci_hwedpool)));
#endif

    if (!softc->ohci_endpoint_pool || !softc->ohci_hwedpool) {
	printf("Could not allocate transfer descriptors\n");
	return -1;
	}

    softc->ohci_endpoint_freelist = NULL;

    for (idx = 0; idx < OHCI_EDPOOL_SIZE; idx++) {
	_ohci_freeept(softc,softc->ohci_endpoint_pool+idx);
	}

    /*
     * Finally the host communications area
     */

    softc->ohci_hcca = KMALLOC(sizeof(ohci_hcca_t),sizeof(ohci_hcca_t));

#if !CPUCFG_COHERENT_DMA
    softc->ohci_hcca = (void *) UNCADDR(PHYSADDR((uint32_t)(softc->ohci_hcca)));
#endif

    memset(softc->ohci_hcca,0,sizeof(ohci_hcca_t));

    return 0;
}


/*  *********************************************************************
    *  ohci_start(bus)
    *  
    *  Start the OHCI controller.  After this routine is called,
    *  the hardware will be operational and ready to accept
    *  descriptors and interrupt calls.
    *  
    *  Input parameters: 
    *  	   bus - bus structure, from ohci_create
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int ohci_start(usbbus_t *bus)
{
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    uint32_t frameint;
    uint32_t reg;
    int idx;

    /*
     * Force a reset to the controller, followed by a short delay
     */

    OHCI_WRITECSR(softc,R_OHCI_CONTROL,V_OHCI_CONTROL_HCFS(K_OHCI_HCFS_RESET));
    usb_delay_ms(bus,OHCI_RESET_DELAY);

    /* Host controller state is now "RESET" */

    /*
     * We need the frame interval later, so get a copy of it now.
     */
    frameint = G_OHCI_FMINTERVAL_FI(OHCI_READCSR(softc,R_OHCI_FMINTERVAL));
    
    /*
     * Reset the host controller.  When you set the HCR bit
     * if self-clears when the reset is complete.
     */

    OHCI_WRITECSR(softc,R_OHCI_CMDSTATUS,M_OHCI_CMDSTATUS_HCR);
    for (idx = 0; idx < 10000; idx++) {
	if (!(OHCI_READCSR(softc,R_OHCI_CMDSTATUS) & M_OHCI_CMDSTATUS_HCR)) break;
	}

    if (OHCI_READCSR(softc,R_OHCI_CMDSTATUS) & M_OHCI_CMDSTATUS_HCR) {
	/* controller never came out of reset */
	return -1;
	}

    /* 
     * Host controller state is now "SUSPEND".  We must exit
     * from this state within 2ms.  (5.1.1.4)
     * 
     * Set up pointers to data structures.
     */

    OHCI_WRITECSR(softc,R_OHCI_HCCA,OHCI_VTOP(softc->ohci_hcca));
    OHCI_WRITECSR(softc,R_OHCI_CONTROLHEADED,softc->ohci_ctl_list->ep_phys);
    OHCI_WRITECSR(softc,R_OHCI_BULKHEADED,softc->ohci_bulk_list->ep_phys);

    /*
     * Our driver is polled, turn off interrupts
     */

    OHCI_WRITECSR(softc,R_OHCI_INTDISABLE,M_OHCI_INT_ALL);

    /*
     * Set up the control register.
     */

    reg = OHCI_READCSR(softc,R_OHCI_CONTROL);

    reg = M_OHCI_CONTROL_PLE | M_OHCI_CONTROL_CLE | M_OHCI_CONTROL_BLE |
	M_OHCI_CONTROL_IE |
	V_OHCI_CONTROL_CBSR(K_OHCI_CBSR_41) | 
	V_OHCI_CONTROL_HCFS(K_OHCI_HCFS_OPERATIONAL);

    OHCI_WRITECSR(softc,R_OHCI_CONTROL,reg);


    /* 
     * controller state is now OPERATIONAL
     */

    reg = OHCI_READCSR(softc,R_OHCI_FMINTERVAL);
    reg &= M_OHCI_FMINTERVAL_FIT;
    reg ^= M_OHCI_FMINTERVAL_FIT;
    reg |= V_OHCI_FMINTERVAL_FSMPS(OHCI_CALC_FSMPS(frameint)) |
	V_OHCI_FMINTERVAL_FI(frameint);
    OHCI_WRITECSR(softc,R_OHCI_FMINTERVAL,reg);

    reg = frameint * 9 / 10;		/* calculate 90% */
    OHCI_WRITECSR(softc,R_OHCI_PERIODICSTART,reg);

    usb_delay_ms(softc->ohci_bus,10);

    /*
     * Remember how many ports we have
     */

    reg = OHCI_READCSR(softc,R_OHCI_RHDSCRA);
    softc->ohci_ndp = G_OHCI_RHDSCRA_NDP(reg);


    /*
     * Enable port power
     */

    OHCI_WRITECSR(softc,R_OHCI_RHSTATUS,M_OHCI_RHSTATUS_LPSC);
    usb_delay_ms(softc->ohci_bus,10);

    return 0;
}


/*  *********************************************************************
    *  _ohci_setupepts(softc)
    *  
    *  Set up the endpoint tree, as described in the OHCI manual.
    *  Basically the hardware knows how to scan lists of lists,
    *  so we build a tree where each level is pointed to by two
    *  parent nodes.  We can choose our scanning rate by attaching
    *  endpoints anywhere within this tree.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error (out of descriptors)
    ********************************************************************* */

static int _ohci_setupepts(ohci_softc_t *softc)
{
    int idx;
    ohci_endpoint_t *e;
    ohci_ed_t *ed;
    ohci_endpoint_t *child;

    /*
     * Set up the list heads for the isochronous, control,
     * and bulk transfer lists.  They don't get the same "tree"
     * treatment that the interrupt devices get.
     * 
     * For the purposes of CFE, it's probably not necessary
     * to be this fancy.  The only device we're planning to
     * talk to is the keyboard and some hubs, which should 
     * have pretty minimal requirements.  It's conceivable
     * that this firmware may find a new home in other
     * devices, so we'll meet halfway and do some things
     * "fancy."
     */

    softc->ohci_isoc_list = _ohci_allocept(softc);
    softc->ohci_ctl_list  = _ohci_allocept(softc);
    softc->ohci_bulk_list = _ohci_allocept(softc);

    /*
     * Set up a tree of empty endpoint descriptors.  This is
     * tree is scanned by the hardware from the leaves up to
     * the root.  Once a millisecond, the hardware picks the
     * next leaf and starts scanning descriptors looking
     * for something to do.  It traverses all of the endpoints
     * along the way until it gets to the root.  
     * 
     * The idea here is if you put a transfer descriptor on the
     * root node, the hardware will see it every millisecond, 
     * since the root will be examined each time.  If you 
     * put the TD on the leaf, it will be 1/32 millisecond.
     * The tree therefore is six levels deep.
     */

    for (idx = 0; idx < OHCI_INTTREE_SIZE; idx++) {
	e = _ohci_allocept(softc);		/* allocated with sKip bit set */
	softc->ohci_edtable[idx] = e;
	child = (idx == 0) ? softc->ohci_isoc_list : softc->ohci_edtable[(idx-1)/2];
	ed = ohci_ed_from_endpoint(softc,e);
	ed->ed_next_ed = BSWAP32(child->ep_phys);
	e->ep_next = child;
	}

    /*
     * We maintain both physical and virtual copies of the interrupt
     * table (leaves of the tree).
     */

    for (idx = 0; idx < OHCI_INTTABLE_SIZE; idx++) {
	child = softc->ohci_edtable[OHCI_INTTREE_SIZE-OHCI_INTTABLE_SIZE+idx];
	softc->ohci_inttable[ohci_revbits[idx]] = child;
	softc->ohci_hcca->hcca_inttable[ohci_revbits[idx]] = BSWAP32(child->ep_phys);
	}

    /*
     * Okay, at this point the tree is built.
     */
    return 0;
}

/*  *********************************************************************
    *  ohci_stop(bus)
    *  
    *  Stop the OHCI hardware.
    *  
    *  Input parameters: 
    *  	   bus - our bus structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_stop(usbbus_t *bus)
{
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;

    OHCI_WRITECSR(softc,R_OHCI_CONTROL,V_OHCI_CONTROL_HCFS(K_OHCI_HCFS_RESET));
}


/*  *********************************************************************
    *  _ohci_queueept(softc,queue,e)
    *  
    *  Add an endpoint to a list of endpoints.  This routine
    *  does things in a particular way according to the OHCI
    *  spec so we can add endpoints while the hardware is running.
    *  
    *  Input parameters: 
    *  	   queue - endpoint descriptor for head of queue
    *  	   e - endpoint to add to queue
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _ohci_queueept(ohci_softc_t *softc,ohci_endpoint_t *queue,ohci_endpoint_t *newept)
{
    ohci_ed_t *qed;
    ohci_ed_t *newed;

    qed = ohci_ed_from_endpoint(softc,queue);
    newed = ohci_ed_from_endpoint(softc,newept);

    newept->ep_next = queue->ep_next;
    newed->ed_next_ed = qed->ed_next_ed;

    queue->ep_next = newept;
    qed->ed_next_ed = BSWAP32(newept->ep_phys);

    if (ohcidebug > 1) ohci_dumped(newed);

}

/*  *********************************************************************
    *  _ohci_deqept(queue,e)
    *  
    *  Remove and endpoint from the list of endpoints.  This
    *  routine does things in a particular way according to
    *  the OHCI specification, since we are operating on
    *  a running list.
    *  
    *  Input parameters: 
    *  	   queue - base of queue to look for endpoint on
    *  	   e - endpoint to remove
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _ohci_deqept(ohci_softc_t *softc,ohci_endpoint_t *queue,ohci_endpoint_t *e)
{
    ohci_endpoint_t *cur;
    ohci_ed_t *cured;
    ohci_ed_t *ed;

    cur = queue;

    while (cur && (cur->ep_next != e)) cur = cur->ep_next;

    if (cur == NULL) {
	printf("Could not remove EP %08X: not on the list!\n",(uint32_t) (intptr_t)e);
	return;
	}

    /*
     * Remove from our regular list
     */

    cur->ep_next = e->ep_next;

    /*
     * now remove from the hardware's list
     */

    cured = ohci_ed_from_endpoint(softc,cur);
    ed = ohci_ed_from_endpoint(softc,e);

    cured->ed_next_ed = ed->ed_next_ed;
}


/*  *********************************************************************
    *  ohci_intr_procdoneq(softc)
    *  
    *  Process the "done" queue for this ohci controller.  As 
    *  descriptors are retired, the hardware links them to the
    *  "done" queue so we can examine the results.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_intr_procdoneq(ohci_softc_t *softc)
{
    uint32_t doneq;
    ohci_transfer_t *transfer;
    ohci_td_t *td;
    int val;
    usbreq_t *ur;

    /*	
     * Get the head of the queue
     */

    doneq = softc->ohci_hcca->hcca_donehead;
    doneq = BSWAP32(doneq);

    td = (ohci_td_t *) OHCI_PTOV(doneq);
    transfer = ohci_transfer_from_td(softc,td);

    /*
     * Process all elements from the queue
     */

    while (doneq) {

	ohci_ed_t *ed;
	ohci_endpoint_t *ept;
	usbreq_t *xur = transfer->t_ref;

	if (ohcidebug > 1) {
	    if (xur) {
		ept = (ohci_endpoint_t *) xur->ur_pipe->up_hwendpoint;
		ed = ohci_ed_from_endpoint(softc,ept);
//		printf("ProcDoneQ:ED [%08X] -> ",ept->ep_phys);
//		ohci_dumped(ed);
		}
	    }

	/* 
	 * Get the pointer to next one before freeing this one
	 */

	if (ohcidebug > 1) {
	    ur = transfer->t_ref;
	    printf("Done(%d): ",ur ? ur->ur_tdcount : -1);
	    ohci_dumptd(td);
	    }

	doneq = BSWAP32(td->td_next_td);

	val = G_OHCI_TD_CC(BSWAP32(td->td_control));

	if (val != 0) printf("[Transfer error: %d]\n",val);

	/*
	 * See if it's time to call the callback.
	 */
	ur = transfer->t_ref;
	if (ur) {
	    ur->ur_status = val;
	    ur->ur_tdcount--;
	    if (BSWAP32(td->td_cbp) == 0) {
		ur->ur_xferred += transfer->t_length; 
		}
	    else {
		ur->ur_xferred += transfer->t_length - 
		    (BSWAP32(td->td_be) - BSWAP32(td->td_cbp) + 1);
		}
	    if (ur->ur_tdcount == 0) {
		/* Noncoherent DMA: need to invalidate, since data is in phys mem */
		OHCI_INVAL_RANGE(ur->ur_buffer,ur->ur_xferred);
		usb_complete_request(ur,val);
		}
	    }


	/*
	 * Free up the request
	 */
	_ohci_freexfer(softc,transfer);


	/*
	 * Advance to the next request.
	 */

	td = (ohci_td_t *) OHCI_PTOV(doneq);
	transfer = ohci_transfer_from_td(softc,td);
	}

}

/*  *********************************************************************
    *  ohci_intr(bus)
    *  
    *  Process pending interrupts for the OHCI controller.
    *  
    *  Input parameters: 
    *  	   bus - our bus structure
    *  	   
    *  Return value:
    *  	   0 if we did nothing
    *  	   nonzero if we did something.
    ********************************************************************* */

static int ohci_intr(usbbus_t *bus)
{
    uint32_t reg;
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;

    /*
     * Read the interrupt status register.
     */

    reg = OHCI_READCSR(softc,R_OHCI_INTSTATUS);

    /*
     * Don't bother doing anything if nothing happened.
     */
    if (reg == 0) {
	return 0;
	}

    /* Scheduling Overruns */
    if (reg & M_OHCI_INT_SO) {
	printf("SchedOverrun\n");
	}

    /* Done Queue */
    if (reg & M_OHCI_INT_WDH) {
	/* printf("DoneQueue\n"); */
	ohci_intr_procdoneq(softc);
	}

    /* Start of Frame */
    if (reg & M_OHCI_INT_SF) {
	/* don't be noisy about this */
	}

    /* Resume Detect */
    if (reg & M_OHCI_INT_RD) {
	printf("ResumeDetect\n");
	}

    /* Unrecoverable errors */
    if (reg & M_OHCI_INT_UE) {
	printf("UnrecoverableError\n");
	}

    /* Frame number overflow */
    if (reg & M_OHCI_INT_FNO) {
	/*printf("FrameNumberOverflow\n"); */
	}

    /* Root Hub Status Change */
    if ((reg & ~softc->ohci_intdisable) & M_OHCI_INT_RHSC) {
	uint32_t reg;
	if (ohcidebug > 0) {
	    printf("RootHubStatusChange: ");
	    reg = OHCI_READCSR(softc,R_OHCI_RHSTATUS);
	    ohci_dumprhstat(reg);
	    reg = OHCI_READCSR(softc,R_OHCI_RHPORTSTATUS(1));
	    ohci_dumpportstat(1,reg);
	    reg = OHCI_READCSR(softc,R_OHCI_RHPORTSTATUS(2));
	    ohci_dumpportstat(2,reg);
	    }
	ohci_roothub_statchg(softc);
	}

    /* Ownership Change */
    if (reg & M_OHCI_INT_OC) {
	printf("OwnershipChange\n");
	}

    /*	
     * Write the value back to the interrupt
     * register to clear the bits that were set.
     */

    OHCI_WRITECSR(softc,R_OHCI_INTSTATUS,reg);

    return 1;
}


/*  *********************************************************************
    *  ohci_delete(bus)
    *  
    *  Remove an OHCI bus structure and all resources allocated to
    *  it (used when shutting down USB)
    *  
    *  Input parameters: 
    *  	   bus - our USB bus structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_delete(usbbus_t *bus)
{
}


/*  *********************************************************************
    *  ohci_create(addr)
    *  
    *  Create a USB bus structure and associate it with our OHCI
    *  controller device.
    *  
    *  Input parameters: 
    *  	   addr - physical address of controller
    *  	   
    *  Return value:
    *  	   usbbus structure pointer
    ********************************************************************* */

static usbbus_t *ohci_create(physaddr_t addr)
{
    int res;
    ohci_softc_t *softc;
    usbbus_t *bus;

    softc = KMALLOC(sizeof(ohci_softc_t),0);
    if (!softc) return NULL;

    bus = KMALLOC(sizeof(usbbus_t),0);
    if (!bus) return NULL;

    memset(softc,0,sizeof(ohci_softc_t));
    memset(bus,0,sizeof(usbbus_t));

    bus->ub_hwsoftc  = (usb_hc_t *) softc;
    bus->ub_hwdisp   = &ohci_driver;

    q_init(&(softc->ohci_rh_intrq));

#ifdef _CFE_
    softc->ohci_regs =  addr;
#else
    softc->ohci_regs = (volatile uint32_t *) addr;
#endif

    softc->ohci_rh_newaddr = -1;
    softc->ohci_bus = bus;
	
    if ((res = _ohci_initpools(softc)) != 0) goto error;
    if ((res = _ohci_setupepts(softc)) != 0) goto error;

    OHCI_WRITECSR(softc,R_OHCI_CONTROL,V_OHCI_CONTROL_HCFS(K_OHCI_HCFS_RESET));

    return bus;

error:
    KFREE(softc);
    return NULL;
}


/*  *********************************************************************
    *  ohci_ept_create(bus,usbaddr,eptnum,mps,flags)
    *  
    *  Create a hardware endpoint structure and attach it to
    *  the hardware's endpoint list.  The hardware manages lists
    *  of queues, and this routine adds a new queue to the appropriate
    *  list of queues for the endpoint in question.  It roughly
    *  corresponds to the information in the OHCI specification.
    *  
    *  Input parameters: 
    *  	   bus - the USB bus we're dealing with
    *  	   usbaddr - USB address (0 means default address)
    *  	   eptnum - the endpoint number
    *  	   mps - the packet size for this endpoint
    *  	   flags - various flags to control endpoint creation
    *  	   
    *  Return value:
    *  	   endpoint structure poihter, or NULL
    ********************************************************************* */

static usb_ept_t *ohci_ept_create(usbbus_t *bus,
				  int usbaddr,
				  int eptnum,
				  int mps,
				  int flags)
{
    uint32_t eptflags;
    ohci_endpoint_t *ept;
    ohci_ed_t *ed;
    ohci_transfer_t *tailtransfer;
    ohci_td_t *tailtd;
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;

    ept = _ohci_allocept(softc);
    ed = ohci_ed_from_endpoint(softc,ept);

    tailtransfer = _ohci_allocxfer(softc);
    tailtd = ohci_td_from_transfer(softc,tailtransfer);

    /*
     * Set up functional address, endpoint number, and packet size
     */

    eptflags = V_OHCI_ED_FA(usbaddr) |
	V_OHCI_ED_EN(eptnum) |
	V_OHCI_ED_MPS(mps) |
	0;

    /*
     * Set up the endpoint type based on the flags
     * passed to us 
     */

    if (flags & UP_TYPE_IN) {
	eptflags |= V_OHCI_ED_DIR(K_OHCI_ED_DIR_IN);
	}
    else if (flags & UP_TYPE_OUT) {
	eptflags |= V_OHCI_ED_DIR(K_OHCI_ED_DIR_OUT);
	}
    else {
	eptflags |= V_OHCI_ED_DIR(K_OHCI_ED_DIR_FROMTD);
	}

    /*
     * Don't forget about lowspeed devices.
     */

    if (flags & UP_TYPE_LOWSPEED) {
	eptflags |= M_OHCI_ED_LOWSPEED;
	}
    
    if (ohcidebug > 0) {
	printf("Create endpoint %d addr %d flags %08X mps %d\n",
	   eptnum,usbaddr,eptflags,mps);
	}

    /*
     * Transfer this info into the endpoint descriptor.
     * No need to flush the cache here, it'll get done when
     * we add to the hardware list.
     */

    ed->ed_control = BSWAP32(eptflags);
    ed->ed_tailp   = BSWAP32(OHCI_VTOP(tailtd));
    ed->ed_headp   = BSWAP32(OHCI_VTOP(tailtd));
    ept->ep_flags = flags;
    ept->ep_mps = mps;
    ept->ep_num = eptnum;

    /*
     * Put it on the right queue
     */

    if (flags & UP_TYPE_CONTROL) {
	_ohci_queueept(softc,softc->ohci_ctl_list,ept);
	}
    else if (flags & UP_TYPE_BULK) {
	_ohci_queueept(softc,softc->ohci_bulk_list,ept);
	}
    else if (flags & UP_TYPE_INTR) {
	_ohci_queueept(softc,softc->ohci_inttable[0],ept);
	}

    return (usb_ept_t *) ept;
}

/*  *********************************************************************
    *  ohci_ept_setaddr(bus,ept,usbaddr)
    *  
    *  Change the functional address for a USB endpoint.  We do this
    *  when we switch the device's state from DEFAULT to ADDRESSED
    *  and we've already got the default pipe open.  This
    *  routine mucks with the descriptor and changes its address
    *  bits.
    *  
    *  Input parameters: 
    *  	   bus - usb bus structure
    *  	   ept - an open endpoint descriptor
    *  	   usbaddr - new address for this endpoint
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_ept_setaddr(usbbus_t *bus,usb_ept_t *uept,int usbaddr)
{
    uint32_t eptflags;
    ohci_endpoint_t *ept = (ohci_endpoint_t *) uept;
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    ohci_ed_t *ed = ohci_ed_from_endpoint(softc,ept);

    eptflags = BSWAP32(ed->ed_control);
    eptflags &= ~M_OHCI_ED_FA;
    eptflags |= V_OHCI_ED_FA(usbaddr);
    ed->ed_control = BSWAP32(eptflags);
}


/*  *********************************************************************
    *  ohci_ept_setmps(bus,ept,mps)
    *  
    *  Set the maximum packet size of this endpoint.  This is
    *  normally used during the processing of endpoint 0 (default
    *  pipe) after we find out how big ep0's packets can be.
    *  
    *  Input parameters: 
    *  	   bus - our USB bus structure
    *  	   ept - endpoint structure
    *  	   mps - new packet size
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_ept_setmps(usbbus_t *bus,usb_ept_t *uept,int mps)
{
    uint32_t eptflags;
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    ohci_endpoint_t *ept = (ohci_endpoint_t *) uept;
    ohci_ed_t *ed = ohci_ed_from_endpoint(softc,ept);

    eptflags = BSWAP32(ed->ed_control);
    eptflags &= ~M_OHCI_ED_MPS;
    eptflags |= V_OHCI_ED_MPS(mps);
    ed->ed_control = BSWAP32(eptflags);
    ept->ep_mps = mps;

}

/*  *********************************************************************
    *  ohci_ept_cleartoggle(bus,ept,mps)
    *  
    *  Clear the data toggle for the specified endpoint.
    *  
    *  Input parameters: 
    *  	   bus - our USB bus structure
    *  	   ept - endpoint structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_ept_cleartoggle(usbbus_t *bus,usb_ept_t *uept)
{
    uint32_t eptflags;
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    ohci_endpoint_t *ept = (ohci_endpoint_t *) uept;
    ohci_ed_t *ed = ohci_ed_from_endpoint(softc,ept);

    eptflags = BSWAP32(ed->ed_headp);
    eptflags &= ~(M_OHCI_ED_HALT | M_OHCI_ED_TOGGLECARRY);
    ed->ed_headp = BSWAP32(eptflags);

    OHCI_WRITECSR(softc,R_OHCI_CMDSTATUS,M_OHCI_CMDSTATUS_CLF);
}

/*  *********************************************************************
    *  ohci_ept_delete(bus,ept)
    *  
    *  Deletes an endpoint from the OHCI controller.  This
    *  routine also completes pending transfers for the 
    *  endpoint and gets rid of the hardware ept (queue base).
    *  
    *  Input parameters: 
    *  	   bus - ohci bus structure
    *  	   ept - endpoint to remove
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_ept_delete(usbbus_t *bus,usb_ept_t *uept)
{
    ohci_endpoint_t *queue;
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    ohci_endpoint_t *ept = (ohci_endpoint_t *) uept;
    ohci_ed_t *ed = ohci_ed_from_endpoint(softc,ept);
    uint32_t framenum;
    uint32_t tdphys;
    usbreq_t *ur;
    ohci_td_t *td;
    ohci_transfer_t *transfer;

    if (ept->ep_flags & UP_TYPE_CONTROL) {
	queue = softc->ohci_ctl_list;
	}
    else if (ept->ep_flags & UP_TYPE_BULK) {
	queue = softc->ohci_bulk_list;
	}
    else if (ept->ep_flags & UP_TYPE_INTR) {
	queue = softc->ohci_inttable[0];
	}
    else {
	printf("Invalid endpoint\n");
	return;
	}


    /*
     * Set the SKIP bit on the endpoint and
     * wait for two SOFs to guarantee that we're
     * not processing this ED anymore.
     */

    ((volatile uint32_t) ed->ed_control) |= BSWAP32(M_OHCI_ED_SKIP);

    framenum = OHCI_READCSR(softc,R_OHCI_FMNUMBER) & 0xFFFF;
    while ((OHCI_READCSR(softc,R_OHCI_FMNUMBER) & 0xFFFF) == framenum) ; /* NULL LOOP */

    framenum = OHCI_READCSR(softc,R_OHCI_FMNUMBER) & 0xFFFF;
    while ((OHCI_READCSR(softc,R_OHCI_FMNUMBER) & 0xFFFF) == framenum) ; /* NULL LOOP */

    /* 
     * Remove endpoint from queue
     */

    _ohci_deqept(softc,queue,ept);

    /*
     * Free/complete the TDs on the queue
     */

    tdphys = BSWAP32(ed->ed_headp) & M_OHCI_ED_PTRMASK;

    while (tdphys != BSWAP32(ed->ed_tailp)) {
	td = (ohci_td_t *) OHCI_PTOV(tdphys);
	tdphys = BSWAP32(td->td_next_td);
	transfer = ohci_transfer_from_td(softc,td);

	ur = transfer->t_ref;
	if (ur) {	
	    ur->ur_status = K_OHCI_CC_CANCELLED;
	    ur->ur_tdcount--;
	    if (ur->ur_tdcount == 0) {
		if (ohcidebug > 0) printf("Completing request due to closed pipe: %p\n",ur);
		usb_complete_request(ur,K_OHCI_CC_CANCELLED);
		}
	    }

	_ohci_freexfer(softc,transfer);
	}

    /*
     * tdphys now points at the tail TD.  Just free it.
     */

    td = (ohci_td_t *) OHCI_PTOV(tdphys);
    _ohci_freexfer(softc,ohci_transfer_from_td(softc,td));

    /*
     * Return endpoint to free pool
     */

    _ohci_freeept(softc,ept);
}



/*  *********************************************************************
    *  ohci_xfer(bus,ept,ur)
    *  
    *  Queue a transfer for the specified endpoint.  Depending on
    *  the transfer type, the transfer may go on one of many queues.
    *  When the transfer completes, a callback will be called.
    *  
    *  Input parameters: 
    *  	   bus - bus structure
    *  	   ept - endpoint descriptor
    *  	   ur - request (includes pointer to user buffer)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

static int ohci_xfer(usbbus_t *bus,usb_ept_t *uept,usbreq_t *ur)
{
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    ohci_endpoint_t *ept = (ohci_endpoint_t *) uept;
    ohci_ed_t *ed = ohci_ed_from_endpoint(softc,ept);
    ohci_transfer_t *newtailtransfer = 0;
    ohci_td_t *newtailtd = NULL;
    ohci_transfer_t *curtransfer;
    ohci_td_t *curtd;
    uint8_t *ptr;
    int len;
    int amtcopy;
    int pktlen;
    uint32_t tdcontrol = 0;

    /*
     * If the destination USB address matches
     * the address of the root hub, shunt the request
     * over to our root hub emulation.
     */
    
    if (ur->ur_dev->ud_address == softc->ohci_rh_addr) {
	return ohci_roothub_xfer(bus,uept,ur);
	}

    /*
     * Set up the TD flags based on the 
     * request type.
     */

//    pktlen = ept->ep_mps;
    pktlen = OHCI_TD_MAX_DATA - 16;

    if (ur->ur_flags & UR_FLAG_SETUP) {
	tdcontrol = V_OHCI_TD_PID(K_OHCI_TD_SETUP) |
	    V_OHCI_TD_DT(K_OHCI_TD_DT_DATA0) |
	    V_OHCI_TD_CC(K_OHCI_CC_NOTACCESSED) |
	    V_OHCI_TD_DI(1);
	}
    else if (ur->ur_flags & UR_FLAG_IN) {
	tdcontrol = V_OHCI_TD_PID(K_OHCI_TD_IN) |
	    V_OHCI_TD_DT(K_OHCI_TD_DT_TCARRY) |
	    V_OHCI_TD_CC(K_OHCI_CC_NOTACCESSED) |
	    V_OHCI_TD_DI(1);
	}
    else if (ur->ur_flags & UR_FLAG_OUT) {
	tdcontrol = V_OHCI_TD_PID(K_OHCI_TD_OUT) |
	    V_OHCI_TD_DT(K_OHCI_TD_DT_TCARRY) |
	    V_OHCI_TD_CC(K_OHCI_CC_NOTACCESSED) |
	    V_OHCI_TD_DI(1);
	}
    else if (ur->ur_flags & UR_FLAG_STATUS_OUT) {
	tdcontrol = V_OHCI_TD_PID(K_OHCI_TD_OUT) |
	    V_OHCI_TD_DT(K_OHCI_TD_DT_DATA1) |
	    V_OHCI_TD_CC(K_OHCI_CC_NOTACCESSED) |
	    V_OHCI_TD_DI(1);
	}
    else if (ur->ur_flags & UR_FLAG_STATUS_IN) {
	tdcontrol = V_OHCI_TD_PID(K_OHCI_TD_IN) |
	    V_OHCI_TD_DT(K_OHCI_TD_DT_DATA1) |
	    V_OHCI_TD_CC(K_OHCI_CC_NOTACCESSED) |
	    V_OHCI_TD_DI(1);
	}
    else {
	printf("Shouldn't happen!\n");
	}

    if (ur->ur_flags & UR_FLAG_SHORTOK) {
	tdcontrol |= M_OHCI_TD_SHORTOK;
	}


    ptr = ur->ur_buffer;
    len = ur->ur_length;
    ur->ur_tdcount = 0;

    if (ohcidebug > 1) {
	printf(">> Queueing xfer addr %d pipe %d ED %08X ptr %016llX length %d\n",
	       ur->ur_dev->ud_address,
	       ur->ur_pipe->up_num,
	       ept->ep_phys,
	       (uint64_t) (uintptr_t) ptr,
	       len);
//	ohci_dumped(ed);
	}

    curtd = OHCI_PTOV(BSWAP32(ed->ed_tailp));
    curtransfer = ohci_transfer_from_td(softc,curtd);

    if (len == 0) {
	newtailtransfer = _ohci_allocxfer(softc);
	newtailtd = ohci_td_from_transfer(softc,newtailtransfer);
	curtd->td_cbp = 0;
	curtd->td_be = 0;
	curtd->td_next_td = BSWAP32(OHCI_VTOP(newtailtd));
	curtd->td_control = BSWAP32(tdcontrol);
	curtransfer->t_next = newtailtransfer;
	curtransfer->t_ref = ur;
	curtransfer->t_length = 0;
	if (ohcidebug > 1) { printf("QueueTD: "); ohci_dumptd(curtd); }
	ur->ur_tdcount++;
	}
    else {
	/* Noncoherent DMA: need to flush user buffer to real memory first */
	OHCI_FLUSH_RANGE(ptr,len);
	while (len > 0) {
	    amtcopy = len;
	    if (amtcopy > pktlen) amtcopy =  pktlen;
	    newtailtransfer = _ohci_allocxfer(softc);
	    newtailtd = ohci_td_from_transfer(softc,newtailtransfer);
	    curtd->td_cbp = BSWAP32(OHCI_VTOP(ptr));
	    curtd->td_be = BSWAP32(OHCI_VTOP(ptr+amtcopy)-1);
	    curtd->td_next_td = BSWAP32(OHCI_VTOP(newtailtd));
	    curtd->td_control = BSWAP32(tdcontrol);
	    curtransfer->t_next = newtailtransfer;
	    curtransfer->t_ref = ur;
	    curtransfer->t_length = amtcopy;
	    if (ohcidebug > 1) { printf("QueueTD: "); ohci_dumptd(curtd); }
	    curtd = newtailtd;
	    curtransfer = ohci_transfer_from_td(softc,curtd);
	    ptr += amtcopy;
	    len -= amtcopy;
	    ur->ur_tdcount++;
	    }
	}

    curtd = OHCI_PTOV(BSWAP32(ed->ed_headp & M_OHCI_ED_PTRMASK));
    ed->ed_tailp = BSWAP32(OHCI_VTOP(newtailtd));

    /*
     * Prod the controller depending on what type of list we put
     * a TD on.
     */

    if (ept->ep_flags & UP_TYPE_BULK) {
	OHCI_WRITECSR(softc,R_OHCI_CMDSTATUS,M_OHCI_CMDSTATUS_BLF);
	}
    else {
	OHCI_WRITECSR(softc,R_OHCI_CMDSTATUS,M_OHCI_CMDSTATUS_CLF);
	}

    return 0;
}

/*  *********************************************************************
    *  Driver structure
    ********************************************************************* */

usb_hcdrv_t ohci_driver = {
    ohci_create,
    ohci_delete,
    ohci_start,
    ohci_stop,
    ohci_intr,
    ohci_ept_create,
    ohci_ept_delete,
    ohci_ept_setmps,
    ohci_ept_setaddr,
    ohci_ept_cleartoggle,
    ohci_xfer
};

/*  *********************************************************************
    *  Root Hub
    *
    *  Data structures and functions
    ********************************************************************* */

/*
 * Data structures and routines to emulate the root hub.
 */
static usb_device_descr_t ohci_root_devdsc = {
    sizeof(usb_device_descr_t),		/* bLength */
    USB_DEVICE_DESCRIPTOR_TYPE,		/* bDescriptorType */
    USBWORD(0x0100),			/* bcdUSB */
    USB_DEVICE_CLASS_HUB,		/* bDeviceClass */
    0,					/* bDeviceSubClass */
    0,					/* bDeviceProtocol */
    64,					/* bMaxPacketSize0 */
    USBWORD(0),				/* idVendor */
    USBWORD(0),				/* idProduct */
    USBWORD(0x0100),			/* bcdDevice */
    1,					/* iManufacturer */
    2,					/* iProduct */
    0,					/* iSerialNumber */
    1					/* bNumConfigurations */
};

static usb_config_descr_t ohci_root_cfgdsc = {
    sizeof(usb_config_descr_t),		/* bLength */
    USB_CONFIGURATION_DESCRIPTOR_TYPE,	/* bDescriptorType */
    USBWORD(
	sizeof(usb_config_descr_t) +
	sizeof(usb_interface_descr_t) +
	sizeof(usb_endpoint_descr_t)),	/* wTotalLength */
    1,					/* bNumInterfaces */
    1,					/* bConfigurationValue */
    0,					/* iConfiguration */
    USB_CONFIG_SELF_POWERED,		/* bmAttributes */
    0					/* MaxPower */
};

static usb_interface_descr_t ohci_root_ifdsc = {
    sizeof(usb_interface_descr_t),	/* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE,	/* bDescriptorType */
    0,					/* bInterfaceNumber */
    0,					/* bAlternateSetting */
    1,					/* bNumEndpoints */
    USB_INTERFACE_CLASS_HUB,		/* bInterfaceClass */
    0,					/* bInterfaceSubClass */
    0,					/* bInterfaceProtocol */
    0					/* iInterface */
};

static usb_endpoint_descr_t ohci_root_epdsc = {
    sizeof(usb_endpoint_descr_t),	/* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,	/* bDescriptorType */
    (USB_ENDPOINT_DIRECTION_IN | 1),	/* bEndpointAddress */
    USB_ENDPOINT_TYPE_INTERRUPT,	/* bmAttributes */
    USBWORD(8),				/* wMaxPacketSize */
    255					/* bInterval */
};

static usb_hub_descr_t ohci_root_hubdsc = {
    USB_HUB_DESCR_SIZE,			/* bLength */
    USB_HUB_DESCRIPTOR_TYPE,		/* bDescriptorType */
    0,					/* bNumberOfPorts */
    USBWORD(0),				/* wHubCharacteristics */
    0,					/* bPowreOnToPowerGood */
    0,					/* bHubControl Current */
    {0}					/* bRemoveAndPowerMask */
};

/*  *********************************************************************
    *  ohci_roothb_strdscr(ptr,str)
    *  
    *  Construct a string descriptor for root hub requests
    *  
    *  Input parameters: 
    *  	   ptr - pointer to where to put descriptor
    *  	   str - regular string to put into descriptor
    *  	   
    *  Return value:
    *  	   number of bytes written to descriptor
    ********************************************************************* */

static int ohci_roothub_strdscr(uint8_t *ptr,char *str)
{
    uint8_t *p = ptr;

    *p++ = strlen(str)*2 + 2;	/* Unicode strings */
    *p++ = USB_STRING_DESCRIPTOR_TYPE;
    while (*str) {
	*p++ = *str++;
	*p++ = 0;
	}
    return (p - ptr);
}

/*  *********************************************************************
    *  ohci_roothub_req(softc,req)
    *  
    *  Handle a descriptor request on the control pipe for the
    *  root hub.  We pretend to be a real root hub here and
    *  return all the standard descriptors.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   req - a usb request (completed immediately)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int ohci_roothub_req(ohci_softc_t *softc,usb_device_request_t *req)
{
    uint8_t *ptr;
    uint16_t wLength;
    uint16_t wValue;
    uint16_t wIndex;
    usb_port_status_t ups;
    usb_hub_descr_t hdsc;
    uint32_t status;
    uint32_t statport;
    uint32_t tmpval;
    int res = 0;

    ptr = softc->ohci_rh_buf;

    wLength = GETUSBFIELD(req,wLength);
    wValue  = GETUSBFIELD(req,wValue);
    wIndex  = GETUSBFIELD(req,wIndex);

    switch (REQSW(req->bRequest,req->bmRequestType)) {

	case REQCODE(USB_REQUEST_GET_STATUS,USBREQ_DIR_IN,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	    *ptr++ = (USB_GETSTATUS_SELF_POWERED & 0xFF);
	    *ptr++ = (USB_GETSTATUS_SELF_POWERED >> 8);
	    break;

	case REQCODE(USB_REQUEST_GET_STATUS,USBREQ_DIR_IN,USBREQ_TYPE_STD,USBREQ_REC_ENDPOINT):
	case REQCODE(USB_REQUEST_GET_STATUS,USBREQ_DIR_IN,USBREQ_TYPE_STD,USBREQ_REC_INTERFACE):
	    *ptr++ = 0;
	    *ptr++ = 0;
	    break;

	case REQCODE(USB_REQUEST_GET_STATUS,USBREQ_DIR_IN,USBREQ_TYPE_CLASS,USBREQ_REC_OTHER):
	    status = OHCI_READCSR(softc,(R_OHCI_RHPORTSTATUS(wIndex)));
	    if (ohcidebug > 0) { printf("RHGetStatus: "); ohci_dumpportstat(wIndex,status);}
	    PUTUSBFIELD((&ups),wPortStatus,(status & 0xFFFF));
	    PUTUSBFIELD((&ups),wPortChange,(status >> 16));
	    memcpy(ptr,&ups,sizeof(ups));
	    ptr += sizeof(ups);
	    break;

	case REQCODE(USB_REQUEST_GET_STATUS,USBREQ_DIR_IN,USBREQ_TYPE_CLASS,USBREQ_REC_DEVICE):
	    *ptr++ = 0;
	    *ptr++ = 0;
	    *ptr++ = 0;
	    *ptr++ = 0;
	    break;

	case REQCODE(USB_REQUEST_CLEAR_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	case REQCODE(USB_REQUEST_CLEAR_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_INTERFACE):
	case REQCODE(USB_REQUEST_CLEAR_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_ENDPOINT):
	    /* do nothing, not supported */
	    break;

	case REQCODE(USB_REQUEST_CLEAR_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_CLASS,USBREQ_REC_OTHER):
	    statport = R_OHCI_RHPORTSTATUS(wIndex);
	    if (ohcidebug> 0) {
		printf("RHClearFeature(%d): ",wValue); ohci_dumpportstat(wIndex,OHCI_READCSR(softc,statport));
		}
	    switch (wValue) {
		case USB_PORT_FEATURE_CONNECTION:
		    break;
		case USB_PORT_FEATURE_ENABLE:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_CCS);
		    break;
		case USB_PORT_FEATURE_SUSPEND:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_POCI);
		    break;
		case USB_PORT_FEATURE_OVER_CURRENT:
		    break;
		case USB_PORT_FEATURE_RESET:
		    break;
		case USB_PORT_FEATURE_POWER:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_LSDA);
		    break;
		case USB_PORT_FEATURE_LOW_SPEED:
		    break;
		case USB_PORT_FEATURE_C_PORT_CONNECTION:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_CSC);
		    break;
		case USB_PORT_FEATURE_C_PORT_ENABLE:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PESC);
		    break;
		case USB_PORT_FEATURE_C_PORT_SUSPEND:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PSSC);
		    break;
		case USB_PORT_FEATURE_C_PORT_OVER_CURRENT:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_OCIC);
		    break;
		case USB_PORT_FEATURE_C_PORT_RESET:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PRSC);
		    break;

		}

	    /*
	     * If we've cleared all of the conditions that
	     * want our attention on the port status,
	     * then we can accept port status interrupts again.
	     */

	    if ((wValue >= USB_PORT_FEATURE_C_PORT_CONNECTION) &&
		(wValue <= USB_PORT_FEATURE_C_PORT_RESET)) {
		status = OHCI_READCSR(softc,statport);
		if ((status & M_OHCI_RHPORTSTAT_ALLC) == 0) {
		    softc->ohci_intdisable &= ~M_OHCI_INT_RHSC;
		    }
		}
	    break;

	case REQCODE(USB_REQUEST_SET_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	case REQCODE(USB_REQUEST_SET_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_INTERFACE):
	case REQCODE(USB_REQUEST_SET_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_ENDPOINT):
	    res = -1;
	    break;

	case REQCODE(USB_REQUEST_SET_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_CLASS,USBREQ_REC_DEVICE):
	    /* nothing */
	    break;

	case REQCODE(USB_REQUEST_SET_FEATURE,USBREQ_DIR_OUT,USBREQ_TYPE_CLASS,USBREQ_REC_OTHER):
	    statport = R_OHCI_RHPORTSTATUS(wIndex);
	    switch (wValue) {
		case USB_PORT_FEATURE_CONNECTION:
		    break;
		case USB_PORT_FEATURE_ENABLE:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PES);
		    break;
		case USB_PORT_FEATURE_SUSPEND:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PSS);
		    break;
		case USB_PORT_FEATURE_OVER_CURRENT:
		    break;
		case USB_PORT_FEATURE_RESET:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PRS);
		    for (;;) {
			usb_delay_ms(softc->ohci_bus,100);
			if (!(OHCI_READCSR(softc,statport) & M_OHCI_RHPORTSTAT_PRS)) break;
			}
		    break;
		case USB_PORT_FEATURE_POWER:
		    OHCI_WRITECSR(softc,statport,M_OHCI_RHPORTSTAT_PPS);
		    break;
		case USB_PORT_FEATURE_LOW_SPEED:
		    break;
		case USB_PORT_FEATURE_C_PORT_CONNECTION:
		    break;
		case USB_PORT_FEATURE_C_PORT_ENABLE:
		    break;
		case USB_PORT_FEATURE_C_PORT_SUSPEND:
		    break;
		case USB_PORT_FEATURE_C_PORT_OVER_CURRENT:
		    break;
		case USB_PORT_FEATURE_C_PORT_RESET:
		    break;

		}
    
	    break;

	case REQCODE(USB_REQUEST_SET_ADDRESS,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	    softc->ohci_rh_newaddr = wValue;
	    break;

	case REQCODE(USB_REQUEST_GET_DESCRIPTOR,USBREQ_DIR_IN,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	    switch (wValue >> 8) {
		case USB_DEVICE_DESCRIPTOR_TYPE:
		    memcpy(ptr,&ohci_root_devdsc,sizeof(ohci_root_devdsc));
		    ptr += sizeof(ohci_root_devdsc);
		    break;
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
		    memcpy(ptr,&ohci_root_cfgdsc,sizeof(ohci_root_cfgdsc));
		    ptr += sizeof(ohci_root_cfgdsc);
		    memcpy(ptr,&ohci_root_ifdsc,sizeof(ohci_root_ifdsc));
		    ptr += sizeof(ohci_root_ifdsc);
		    memcpy(ptr,&ohci_root_epdsc,sizeof(ohci_root_epdsc));
		    ptr += sizeof(ohci_root_epdsc);
		    break;
		case USB_STRING_DESCRIPTOR_TYPE:
		    switch (wValue & 0xFF) {
			case 1:	 
			    ptr += ohci_roothub_strdscr(ptr,"Generic");
			    break;
			case 2:
			    ptr += ohci_roothub_strdscr(ptr,"Root Hub");
			    break;
			default:
			    *ptr++ = 0;
			    break;
			}
		    break;
		default:
		    res = -1;
		    break;
		}
	    break;

	case REQCODE(USB_REQUEST_GET_DESCRIPTOR,USBREQ_DIR_IN,USBREQ_TYPE_CLASS,USBREQ_REC_DEVICE):
	    memcpy(&hdsc,&ohci_root_hubdsc,sizeof(hdsc));
	    hdsc.bNumberOfPorts = softc->ohci_ndp;
	    status = OHCI_READCSR(softc,R_OHCI_RHDSCRA);
	    tmpval = 0;
	    if (status & M_OHCI_RHDSCRA_NPS) tmpval |= USB_HUBCHAR_PWR_NONE;
	    if (status & M_OHCI_RHDSCRA_PSM) tmpval |= USB_HUBCHAR_PWR_GANGED;
	    else tmpval |= USB_HUBCHAR_PWR_IND;
	    PUTUSBFIELD((&hdsc),wHubCharacteristics,tmpval);
	    tmpval = G_OHCI_RHDSCRA_POTPGT(status);
	    hdsc.bPowerOnToPowerGood = tmpval;
	    hdsc.bDescriptorLength = USB_HUB_DESCR_SIZE + 1;
	    status = OHCI_READCSR(softc,R_OHCI_RHDSCRB);
	    hdsc.bRemoveAndPowerMask[0] = (uint8_t) status;
	    memcpy(ptr,&hdsc,sizeof(hdsc));
	    ptr += sizeof(hdsc);
	    break;

	case REQCODE(USB_REQUEST_SET_DESCRIPTOR,USBREQ_DIR_OUT,USBREQ_TYPE_CLASS,USBREQ_REC_DEVICE):
	    /* nothing */
	    break;

	case REQCODE(USB_REQUEST_GET_CONFIGURATION,USBREQ_DIR_IN,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	    *ptr++ = softc->ohci_rh_conf;
	    break;

	case REQCODE(USB_REQUEST_SET_CONFIGURATION,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_DEVICE):
	    softc->ohci_rh_conf = wValue;
	    break;

	case REQCODE(USB_REQUEST_GET_INTERFACE,USBREQ_DIR_IN,USBREQ_TYPE_STD,USBREQ_REC_INTERFACE):
	    *ptr++ = 0;
	    break;

	case REQCODE(USB_REQUEST_SET_INTERFACE,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_INTERFACE):
	    /* nothing */
	    break;

	case REQCODE(USB_REQUEST_SYNC_FRAME,USBREQ_DIR_OUT,USBREQ_TYPE_STD,USBREQ_REC_ENDPOINT):
	    /* nothing */
	    break;
	}

    softc->ohci_rh_ptr = softc->ohci_rh_buf;
    softc->ohci_rh_len = ptr - softc->ohci_rh_buf;

    return res;
}

/*  *********************************************************************
    *  ohci_roothub_statchg(softc)
    *  
    *  This routine is called from the interrupt service routine
    *  (well, polling routine) for the ohci controller.  If the 
    *  controller notices a root hub status change, it dequeues an
    *  interrupt transfer from the root hub's queue and completes
    *  it here.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void ohci_roothub_statchg(ohci_softc_t *softc)
{
    usbreq_t *ur;
    uint32_t status;
    uint8_t portstat = 0;
    int idx;

    /* Note: this only works up to 8 ports */
    for (idx = 1; idx <= softc->ohci_ndp; idx++) {
	status = OHCI_READCSR(softc,R_OHCI_RHPORTSTATUS(idx));
	if (status & M_OHCI_RHPORTSTAT_ALLC) {
	    portstat = (1<<idx);
	    }
	}

    if (portstat != 0) {
	softc->ohci_intdisable |= M_OHCI_INT_RHSC;
	}

    ur = (usbreq_t *) q_deqnext(&(softc->ohci_rh_intrq));
    if (!ur) return;		/* no requests pending, ignore it */

    memset(ur->ur_buffer,0,ur->ur_length);
    ur->ur_buffer[0] = portstat;
    ur->ur_xferred = ur->ur_length;

    usb_complete_request(ur,0);
}

/*  *********************************************************************
    *  ohci_roothub_xfer(softc,req)
    *  
    *  Handle a root hub xfer - ohci_xfer transfers control here
    *  if we detect the address of the root hub - no actual transfers
    *  go out on the wire, we just handle the requests directly to
    *  make it look like a hub is attached.
    *  
    *  This seems to be common practice in the USB world, so we do
    *  it here too.
    *  
    *  Input parameters: 
    *  	   softc - our OHCI controller structure
    *  	   req - usb request destined for host controller
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

static int ohci_roothub_xfer(usbbus_t *bus,usb_ept_t *uept,usbreq_t *ur)
{
    ohci_softc_t *softc = (ohci_softc_t *) bus->ub_hwsoftc;
    ohci_endpoint_t *ept = (ohci_endpoint_t *) uept;
    int res;

    switch (ept->ep_num) {

	/*
	 * CONTROL ENDPOINT
	 */
	case 0:

	    /*
	     * Three types of transfers:  OUT (SETUP), IN (data), or STATUS.
	     * figure out which is which.
	     */

	    if (ur->ur_flags & UR_FLAG_SETUP) {
		/*
		 * SETUP packet - this is an OUT request to the control
		 * pipe.  We emulate the hub request here.
		 */
		usb_device_request_t *req;

		req = (usb_device_request_t *) ur->ur_buffer;

		res = ohci_roothub_req(softc,req);
		if (res != 0) printf("Root hub request returned an error\n");

		ur->ur_xferred = ur->ur_length;
		ur->ur_status = 0;
		usb_complete_request(ur,0);
		}

	    else if (ur->ur_flags & UR_FLAG_STATUS_IN) {
		/*
		 * STATUS IN : it's sort of like a dummy IN request
		 * to acknowledge a SETUP packet that otherwise has no 
		 * status.  Just complete the usbreq.
		 */

		if (softc->ohci_rh_newaddr != -1) {
		    softc->ohci_rh_addr = softc->ohci_rh_newaddr;
		    softc->ohci_rh_newaddr = -1;
		    }

		ur->ur_status = 0;
		ur->ur_xferred = 0;
		usb_complete_request(ur,0);
		}

	    else if (ur->ur_flags & UR_FLAG_STATUS_OUT) {
		/*
		 * STATUS OUT : it's sort of like a dummy OUT request
		 */
		ur->ur_status = 0;
		ur->ur_xferred = 0;
		usb_complete_request(ur,0);
		}

	    else if (ur->ur_flags & UR_FLAG_IN) {
		/*
		 * IN : return data from the root hub
		 */
		int amtcopy;

		amtcopy = softc->ohci_rh_len;
		if (amtcopy > ur->ur_length) amtcopy = ur->ur_length;

		memcpy(ur->ur_buffer,softc->ohci_rh_ptr,amtcopy);

		softc->ohci_rh_ptr += amtcopy;
		softc->ohci_rh_len -= amtcopy;

		ur->ur_status = 0;
		ur->ur_xferred = amtcopy;
		usb_complete_request(ur,0);
		}

	    else {
		printf("Unknown root hub transfer type\n");
		return -1;
		}
	    break;

	/*
	 * INTERRUPT ENDPOINT 
	 */

	case 1:			/* interrupt pipe */
	    if (ur->ur_flags & UR_FLAG_IN) {
		q_enqueue(&(softc->ohci_rh_intrq),(queue_t *) ur);
		}
 	    break;

	}


    return 0;
}
