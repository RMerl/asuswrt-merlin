/*  *********************************************************************
    *  SB1250_ETHERNET
    *
    *  CFE Ethernet Driver		File: DEV_SB1250_ETHERNET.C
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  This is the console monitor Ethernet driver for the SB1250
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
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_console.h"
#include "cfe_timer.h"
#include "cfe_error.h"

#include "env_subr.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_mac.h"
#include "sb1250_dma.h"
#include "mii.h"

/*  *********************************************************************
    *  Simple types
    ********************************************************************* */

#ifdef __long64
typedef volatile uint64_t sbeth_port_t;
typedef uint64_t sbeth_physaddr_t;
#define SBETH_PORT(x) PHYS_TO_K1(x)
#else
typedef volatile uint32_t sbeth_port_t;
typedef uint32_t sbeth_physaddr_t;
#define SBETH_PORT(x) PHYS_TO_K1(x)
#endif

#define SBETH_READCSR(t) (*((volatile uint64_t *) (t)))
#define SBETH_WRITECSR(t,v) *((volatile uint64_t *) (t)) = (v)

#define SBETH_MAX_TXDESCR	8
#define SBETH_MAX_RXDESCR	8
#define SBETH_MIN_RCV_RING	4
#define SBETH_PKTPOOL_SIZE	16
#define SBETH_DMA_CHANNELS	1
#define SBETH_PKT_SIZE	        1518
#define SBETH_PKTBUF_SIZE	2048

#define SBETH_ADDR_LEN		6


typedef enum { sbeth_speed_10, sbeth_speed_100, 
	       sbeth_speed_1000 } sbeth_speed_t;

typedef enum { sbeth_duplex_half,
	       sbeth_duplex_full } sbeth_duplex_t;

typedef enum { sbeth_fc_disabled, sbeth_fc_frame,
	       sbeth_fc_collision, sbeth_fc_carrier } sbeth_fc_t;

typedef enum { sbeth_state_uninit, sbeth_state_off, sbeth_state_on, 
	       sbeth_state_broken } sbeth_state_t;

typedef uint64_t  sbeth_enetaddr_t;

typedef struct sbeth_s sbeth_t;		/* forward reference */

static int sbeth_setspeed(sbeth_t *s,sbeth_speed_t speed);
static int sbeth_set_duplex(sbeth_t *s,sbeth_duplex_t duplex,sbeth_fc_t fc);

#define SBETH_MIIPOLL_TIMER	(4*CFE_HZ)

/*  *********************************************************************
    *  Descriptor structure
    ********************************************************************* */

typedef struct sbdmadscr_s {
    uint64_t  dscr_a;
    uint64_t  dscr_b;
} sbdmadscr_t;

/*  *********************************************************************
    *  DMA structure
    ********************************************************************* */

typedef struct sbethdma_s {

    /* 
     * This stuff is used to identify the channel and the registers
     * associated with it.
     */

    sbeth_t         *sbdma_eth;	        /* back pointer to associated MAC */
    int              sbdma_channel;	/* channel number */
    int		     sbdma_txdir;       /* direction (1=transmit) */
    int		     sbdma_maxdescr;	/* total # of descriptors in ring */
    sbeth_port_t     sbdma_config0;	/* DMA config register 0 */
    sbeth_port_t     sbdma_config1;	/* DMA config register 1 */
    sbeth_port_t     sbdma_dscrbase;	/* Descriptor base address */
    sbeth_port_t     sbdma_dscrcnt;     /* Descriptor count register */
    sbeth_port_t     sbdma_curdscr;	/* current descriptor address */

    /*
     * This stuff is for maintenance of the ring
     */

    sbdmadscr_t     *sbdma_dscrtable;	/* base of descriptor table */
    sbdmadscr_t     *sbdma_dscrtable_end; /* end of descriptor table */

    void            **sbdma_ctxtable;   /* context table, one per descr */

    int		     sbdma_onring;	/* count of packets on ring */

    sbeth_physaddr_t sbdma_dscrtable_phys; /* and also the phys addr */
    sbdmadscr_t     *sbdma_addptr;	/* next dscr for sw to add */
    sbdmadscr_t     *sbdma_remptr;	/* next dscr for sw to remove */

    void	   (*sbdma_upcall)(void *ifctx,int chan,void *ctx,
				   uint64_t status,unsigned int length);
} sbethdma_t;

typedef struct sbeth_pkt_s {
    struct sbeth_pkt_s *next;
    void *devctx;
    unsigned char *buffer;
    int length;
    /* packet data goes here */
} sbeth_pkt_t;

/*  *********************************************************************
    *  Ethernet controller structure
    ********************************************************************* */

struct sbeth_s {
    cfe_devctx_t    *sbe_devctx;
    sbeth_port_t     sbe_baseaddr;	/* base address */

    sbeth_state_t    sbe_state;         /* current state */

    sbeth_port_t     sbe_macenable;	/* MAC Enable Register */
    sbeth_port_t     sbe_maccfg;	/* MAC Configuration Register */
    sbeth_port_t     sbe_fifocfg;	/* FIFO configuration register */
    sbeth_port_t     sbe_framecfg;	/* Frame configuration register */
    sbeth_port_t     sbe_rxfilter;	/* receive filter register */
    sbeth_port_t     sbe_isr;		/* Interrupt status register */
    sbeth_port_t     sbe_imr;		/* Interrupt mask register */
    sbeth_port_t     sbe_mdio;		/* PHY control stuff */

    sbeth_speed_t    sbe_speed;		/* current speed */
    sbeth_duplex_t   sbe_duplex;	/* current duplex */
    sbeth_fc_t       sbe_fc;		/* current flow control setting */
    int		     sbe_rxflags;	/* received packet flags */
    int		     sbe_autospeed;	/* true for automatic speed setting */
    int		     sbe_curspeed;	/* value for GET SPEED ioctl */
    int		     sbe_loopback;	/* IOCTL LOOPBACK stuff */
    int		     sbe_linkstat;	/* Current link status */

    sbethdma_t       sbe_txdma[SBETH_DMA_CHANNELS];	/* one for each channel */
    sbethdma_t       sbe_rxdma[SBETH_DMA_CHANNELS];
    void	    *sbe_ifctx;

    int              sbe_minrxring;	/* min packets to keep on RX ring */

    sbeth_pkt_t	    *sbe_rxqueue;	/* received packet queue */

    sbeth_pkt_t     *sbe_freelist;	/* free packet list */

    unsigned char   *sbe_pktpool;

    unsigned char    sbe_hwaddr[SBETH_ADDR_LEN];

    int		     sbe_phyaddr;
    int		     sbe_zerormon;

    uint32_t	     sbe_phy_oldbmsr;
    uint32_t	     sbe_phy_oldbmcr;
    uint32_t	     sbe_phy_oldanlpar;
    uint32_t	     sbe_phy_oldk1stsr;

    int64_t	     sbe_linkstat_timer;
    int		     fifo_mode;		/* true if in packet fifo mode */

};


/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int sbeth_transmit(sbeth_t *s,int chan,unsigned char *pkt,int length,void *arg);
static int sbeth_addrcvbuf(sbeth_t *s,int chan,unsigned char *pkt,int length,void *arg);
static int sbeth_initctx(sbeth_t *s,unsigned long baseaddr,void *ifctx);

static void sbeth_start(sbeth_t *s);
static void sbeth_stop(sbeth_t *s);

static void sbeth_initfreelist(sbeth_t *s);
static sbeth_pkt_t *sbeth_alloc_pkt(sbeth_t *s);
static void sbeth_free_pkt(sbeth_t *s,sbeth_pkt_t *pkt);
static void sbeth_tx_callback(void *ifctx,int chan,void *ctx,
			      uint64_t status,unsigned int pktsize);
static void sbeth_rx_callback(void *ifctx,int chan,void *ctx,
			      uint64_t status,unsigned int pktsize);
static void sbeth_fillrxring(sbeth_t *s,int chan);
static void sb1250_ether_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr);

static void sbeth_setaddr(sbeth_t *s,uint8_t *addr);

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define sbdma_nextbuf(d,f) ((((d)->f+1) == (d)->sbdma_dscrtable_end) ? \
			  (d)->sbdma_dscrtable : (d)->f+1)

#define SBDMA_CACHESIZE 32		/* wants to be somewhere else */
#define SBDMA_NUMCACHEBLKS(x) ((x+SBDMA_CACHESIZE-1)/SBDMA_CACHESIZE)

#define STRAP_PHY1	0x0800
#define STRAP_NCMODE	0x0400
#define STRAP_MANMSCFG	0x0200
#define STRAP_ANENABLE	0x0100
#define STRAP_MSVAL	0x0080
#define STRAP_1KHDXADV	0x0010
#define STRAP_1KFDXADV	0x0008
#define STRAP_100ADV	0x0004
#define STRAP_SPEEDSEL	0x0000
#define STRAP_SPEED100	0x0001

#define PHYSUP_SPEED1000 0x10
#define PHYSUP_SPEED100  0x08
#define PHYSUP_SPEED10   0x00
#define PHYSUP_LINKUP	 0x04
#define PHYSUP_FDX       0x02

#define M_MAC_MDIO_DIR_OUTPUT	0		/* for clarity */


/* ********************************************************************** */



/*  *********************************************************************
    *  SBETH_MII_SYNC(s)
    *  
    *  Synchronize with the MII - send a pattern of bits to the MII
    *  that will guarantee that it is ready to accept a command.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_mii_sync(sbeth_t *s)
{
    int cnt;
    uint64_t bits;
    int mac_mdio_genc;

    mac_mdio_genc = SBETH_READCSR(s->sbe_mdio) & M_MAC_GENC;

    bits = M_MAC_MDIO_DIR_OUTPUT | M_MAC_MDIO_OUT;

    SBETH_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);

    for (cnt = 0; cnt < 32; cnt++) {
	SBETH_WRITECSR(s->sbe_mdio,bits | M_MAC_MDC | mac_mdio_genc);
	SBETH_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);
	}

}

/*  *********************************************************************
    *  SBETH_MII_SENDDATA(s,data,bitcnt)
    *  
    *  Send some bits to the MII.  The bits to be sent are right-
    *  justified in the 'data' parameter.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   data - data to send
    *  	   bitcnt - number of bits to send
    ********************************************************************* */

static void sbeth_mii_senddata(sbeth_t *s,unsigned int data, int bitcnt)
{
    int i;
    uint64_t bits;
    unsigned int curmask;
    int mac_mdio_genc;

    mac_mdio_genc = SBETH_READCSR(s->sbe_mdio) & M_MAC_GENC;

    bits = M_MAC_MDIO_DIR_OUTPUT;
    SBETH_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);

    curmask = 1 << (bitcnt - 1);

    for (i = 0; i < bitcnt; i++) {
	if (data & curmask) bits |= M_MAC_MDIO_OUT;
	else bits &= ~M_MAC_MDIO_OUT;
	SBETH_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);
	SBETH_WRITECSR(s->sbe_mdio,bits | M_MAC_MDC | mac_mdio_genc);
	SBETH_WRITECSR(s->sbe_mdio,bits | mac_mdio_genc);
	curmask >>= 1;
	}
}



/*  *********************************************************************
    *  SBETH_MII_READ(s,phyaddr,regidx)
    *  
    *  Read a PHY register.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   phyaddr - PHY's address
    *  	   regidx = index of register to read
    *  	   
    *  Return value:
    *  	   value read, or 0 if an error occured.
    ********************************************************************* */

static unsigned int sbeth_mii_read(sbeth_t *s,int phyaddr,int regidx)
{
    int idx;
    int error;
    int regval;
    int mac_mdio_genc;

    /*
     * Synchronize ourselves so that the PHY knows the next
     * thing coming down is a command
     */

    sbeth_mii_sync(s);

    /*
     * Send the data to the PHY.  The sequence is
     * a "start" command (2 bits)
     * a "read" command (2 bits)
     * the PHY addr (5 bits)
     * the register index (5 bits)
     */

    sbeth_mii_senddata(s,MII_COMMAND_START, 2);
    sbeth_mii_senddata(s,MII_COMMAND_READ, 2);
    sbeth_mii_senddata(s,phyaddr, 5);
    sbeth_mii_senddata(s,regidx, 5);

    mac_mdio_genc = SBETH_READCSR(s->sbe_mdio) & M_MAC_GENC;

    /* 
     * Switch the port around without a clock transition.
     */
    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);

    /*
     * Send out a clock pulse to signal we want the status
     */

    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | M_MAC_MDC | mac_mdio_genc);
    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);

    /* 
     * If an error occured, the PHY will signal '1' back
     */
    error = SBETH_READCSR(s->sbe_mdio) & M_MAC_MDIO_IN;

    /* 
     * Issue an 'idle' clock pulse, but keep the direction
     * the same.
     */
    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | M_MAC_MDC | mac_mdio_genc);
    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);

    regval = 0;

    for (idx = 0; idx < 16; idx++) {
	regval <<= 1;

	if (error == 0) {
	    if (SBETH_READCSR(s->sbe_mdio) & M_MAC_MDIO_IN) regval |= 1;
	    }

	SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | M_MAC_MDC | mac_mdio_genc);
	SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_INPUT | mac_mdio_genc);
	}

    /* Switch back to output */
    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_OUTPUT | mac_mdio_genc);

    if (error == 0) return regval;
    return 0;
}


/*  *********************************************************************
    *  SBETH_MII_WRITE(s,phyaddr,regidx,regval)
    *  
    *  Write a value to a PHY register.
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   phyaddr - PHY to use
    *  	   regidx - register within the PHY
    *  	   regval - data to write to register
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void sbeth_mii_write(sbeth_t *s,int phyaddr,int regidx,
		     unsigned int regval);
void sbeth_mii_write(sbeth_t *s,int phyaddr,int regidx,
			    unsigned int regval)
{
    int mac_mdio_genc;
    
    sbeth_mii_sync(s);

    sbeth_mii_senddata(s,MII_COMMAND_START,2);
    sbeth_mii_senddata(s,MII_COMMAND_WRITE,2);
    sbeth_mii_senddata(s,phyaddr, 5);
    sbeth_mii_senddata(s,regidx, 5);
    sbeth_mii_senddata(s,MII_COMMAND_ACK,2);
    sbeth_mii_senddata(s,regval,16);

    mac_mdio_genc = SBETH_READCSR(s->sbe_mdio) & M_MAC_GENC;
    
    SBETH_WRITECSR(s->sbe_mdio,M_MAC_MDIO_DIR_OUTPUT | mac_mdio_genc);
}


/*  *********************************************************************
    *  SBDMA_INITCHAN(s,d)
    *  
    *  Initialize the DMA channel, programming the CSRs to the 
    *  values calculated in the SBDMA_INITCTX routine.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   d - sbdma structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void sbdma_initchan(sbeth_t *s,
			   sbethdma_t *d)
{
    /*
     * Turn on the DMA channel
     */

    SBETH_WRITECSR(d->sbdma_config1,0);

    SBETH_WRITECSR(d->sbdma_dscrbase,d->sbdma_dscrtable_phys);

    SBETH_WRITECSR(d->sbdma_config0,
		   V_DMA_RINGSZ(d->sbdma_maxdescr) |
		   0);

}

/*  *********************************************************************
    *  SBDMA_INITCTX(s,d,chan,txrx,maxdescr,callback)
    *  
    *  Initialize a DMA channel context.  Since there are potentially
    *  eight DMA channels per MAC, it's nice to do this in a standard
    *  way.  
    *  
    *  Input parameters: 
    *  	   s - sbeth_t structure (pointer to a MAC)
    *  	   d - sbethdma_t structure (DMA channel context)
    *  	   chan - channel number (0..1 right now)
    *  	   txrx - Identifies DMA_TX or DMA_RX for channel direction
    *  	   maxdescr - number of descriptors to allocate for the ring
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbdma_initctx(sbeth_t *s,
			  sbethdma_t *d,
			  int chan,
			  int txrx,
			  int maxdescr,
			  void (*callback)(void *,int,void *,uint64_t,unsigned int))
{
    /* 
     * Save away interesting stuff in the structure 
     */

    d->sbdma_eth       = s;
    d->sbdma_channel   = chan;
    d->sbdma_txdir     = txrx;
    d->sbdma_maxdescr  = maxdescr;

    /* 
     * initialize register pointers 
     */

    d->sbdma_config0 = SBETH_PORT(s->sbe_baseaddr + R_MAC_DMA_REGISTER(txrx,chan,R_MAC_DMA_CONFIG0));
    d->sbdma_config1 = SBETH_PORT(s->sbe_baseaddr + R_MAC_DMA_REGISTER(txrx,chan,R_MAC_DMA_CONFIG1));
    d->sbdma_dscrbase = SBETH_PORT(s->sbe_baseaddr + R_MAC_DMA_REGISTER(txrx,chan,R_MAC_DMA_DSCR_BASE));
    d->sbdma_dscrcnt = SBETH_PORT(s->sbe_baseaddr + R_MAC_DMA_REGISTER(txrx,chan,R_MAC_DMA_DSCR_CNT));
    d->sbdma_curdscr = 	SBETH_PORT(s->sbe_baseaddr + R_MAC_DMA_REGISTER(txrx,chan,R_MAC_DMA_CUR_DSCRADDR));

    /*
     * initialize the ring
     */

    d->sbdma_dscrtable = (sbdmadscr_t *) 
	KMALLOC(maxdescr*sizeof(sbdmadscr_t),sizeof(sbdmadscr_t));
    d->sbdma_dscrtable_end = d->sbdma_dscrtable + maxdescr;

    d->sbdma_dscrtable_phys = K1_TO_PHYS((sbeth_physaddr_t) d->sbdma_dscrtable);
    d->sbdma_addptr = d->sbdma_dscrtable;
    d->sbdma_remptr = d->sbdma_dscrtable;

    d->sbdma_ctxtable = (void **) 
	KMALLOC(maxdescr*sizeof(void *),sizeof(void *));

    /*
     * install callback
     */

    d->sbdma_upcall = callback;


}



/*  *********************************************************************
    *  SBDMA_RESET(d)
    *  
    *  Reset the software-maintained state for the specified
    *  DMA channel.
    *  
    *  Input parameters: 
    *  	   d - dma channel
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbdma_reset(sbethdma_t *d)
{
    d->sbdma_addptr = d->sbdma_dscrtable;
    d->sbdma_remptr = d->sbdma_dscrtable;
    d->sbdma_onring = 0;
}

/*  *********************************************************************
    *  SBDMA_PROCBUFFERS(d,procfunc)
    *  
    *  Process "completed" buffers on the specified DMA channel.  
    *  This is normally called within the interrupt service routine.
    *  Note that this isn't really ideal for priority channels, since
    *  it processes all of the packets on a given channel before 
    *  returning. 
    *
    *  Input parameters: 
    *  	   d - DMA channel context
    *  	   procfunc - routine to call for each completed buffer.  This
    *  	              is called with the context for the completed buffer,
    *  	              the status from the descriptor, and the length from
    *  	              the descriptor.
    *  	   
    *  Return value:
    *  	   number of packets processed.
    ********************************************************************* */

static int sbdma_procbuffers(sbethdma_t *d,
			     void (*procfunc)(void *ifctx,int chan,void *ctx,
					      uint64_t status,
					      unsigned int pktlen))
{
    int curidx;
    int hwidx;
    int count = 0;
    sbdmadscr_t *dsc;

    for (;;) {
	/* 
	 * figure out where we are (as an index) and where
	 * the hardware is (also as an index)
	 *
	 * This could be done faster if (for example) the 
	 * descriptor table was page-aligned and contiguous in
	 * both virtual and physical memory -- you could then
	 * just compare the low-order bits of the virtual address
	 * (sbdma_remptr) and the physical address (sbdma_curdscr CSR)
	 */

	curidx = d->sbdma_remptr - d->sbdma_dscrtable;
	{
	    uint64_t tmp;
	    tmp = SBETH_READCSR(d->sbdma_curdscr);
	    if (!tmp) {
	        break;
	        }
	    hwidx = (int) (((tmp & M_DMA_CURDSCR_ADDR) -
				d->sbdma_dscrtable_phys) / sizeof(sbdmadscr_t));
	}

	/*
	 * If they're the same, that means we've processed all
	 * of the descriptors up to (but not including) the one that
	 * the hardware is working on right now.
	 */

	if (curidx == hwidx) break;

	/*
	 * Remove packet from the on-ring count.
	 */

	d->sbdma_onring--;

	/*
	 * Otherwise, issue the upcall.
	 */


	dsc = &(d->sbdma_dscrtable[curidx]);
	(*procfunc)(d->sbdma_eth->sbe_ifctx,
		    d->sbdma_channel,
		    d->sbdma_ctxtable[curidx],
		    dsc->dscr_a & M_DMA_DSCRA_STATUS,
		    (int)G_DMA_DSCRB_PKT_SIZE(dsc->dscr_b));
	count++;

	/* 
	 * .. and advance to the next buffer.
	 */

	d->sbdma_remptr = sbdma_nextbuf(d,sbdma_remptr);

	}

    return count;
}

/*  *********************************************************************
    *  SBDMA_ADDBUFFER(d,ptr,length,ctx)
    *  
    *  Add a buffer to the specified DMA channel.  For transmit channels,
    *  this causes a transmission to start.  For receive channels,
    *  this queues a buffer for inbound packets.
    *  
    *  Input parameters: 
    *  	   d - DMA channel descriptor
    *  	   ptr - pointer to buffer (must by physically contiguous)
    *  	   length - length of buffer
    *  	   ctx - arbitrary data to be passed back when descriptor completes
    *  	         (for example, mbuf pointers, etc.)
    *  	   
    *  Return value:
    *  	   0 if buffer could not be added (ring is full)
    *  	   1 if buffer added successfully
    ********************************************************************* */

static int sbdma_addbuffer(sbethdma_t *d,uint8_t *ptr,int length,void *ctx)
{
    sbdmadscr_t *dsc;
    sbdmadscr_t *nextdsc;

    sbeth_t         *s = d->sbdma_eth;

    /* get pointer to our current place in the ring */
    dsc = d->sbdma_addptr;
    nextdsc = sbdma_nextbuf(d,sbdma_addptr);

    /*
     * figure out if the ring is full - if the next descriptor
     * is the same as the one that we're going to remove from
     * the ring, the ring is full
     */

    if (nextdsc == d->sbdma_remptr) {
	return 0;
	}

    /*
     * fill in the descriptor 
     */

    if (d->sbdma_txdir) {
	/* transmitting: set outbound options and length */
	dsc->dscr_a = K1_TO_PHYS((sbeth_physaddr_t) ptr) |
	    V_DMA_DSCRA_A_SIZE(SBDMA_NUMCACHEBLKS(((uint64_t) length))) |
	    M_DMA_DSCRA_INTERRUPT |
	    M_DMA_ETHTX_SOP;

	if (s->fifo_mode) {
	    dsc->dscr_b = V_DMA_DSCRB_OPTIONS(K_DMA_ETHTX_NOMODS) |
		V_DMA_DSCRB_PKT_SIZE(length);	    
	    }
	else {
	    dsc->dscr_b = V_DMA_DSCRB_OPTIONS(K_DMA_ETHTX_APPENDCRC_APPENDPAD) |
		V_DMA_DSCRB_PKT_SIZE(length);
	    }

	}
    else {
	/* receiving: no options */
	dsc->dscr_a = K1_TO_PHYS((sbeth_physaddr_t) ptr) |
	    V_DMA_DSCRA_A_SIZE(SBDMA_NUMCACHEBLKS(((uint64_t) length))) |
	    M_DMA_DSCRA_INTERRUPT;
	dsc->dscr_b = 0;
	}

    /*
     * fill in the context 
     */

    d->sbdma_ctxtable[dsc-d->sbdma_dscrtable] = ctx;

    /* 
     * point at next packet 
     */

    d->sbdma_addptr = nextdsc;

    /* 
     * Give the packet to the hardware
     */

    d->sbdma_onring++;
    SBETH_WRITECSR(d->sbdma_dscrcnt,1);	

    return 1;					/* we did it */
}


/*  *********************************************************************
    *  SBETH_INITFREELIST(s)
    *  
    *  Initialize the buffer free list for this mac.  The memory
    *  allocated to the free list is carved up and placed on a linked
    *  list of buffers for use by the mac.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_initfreelist(sbeth_t *s)
{
    int idx;
    unsigned char *ptr;
    sbeth_pkt_t *pkt;

    s->sbe_freelist = NULL;

    /* Must empty rxqueue, as we're about to free all the pkts on it */
    s->sbe_rxqueue = NULL;

    ptr = s->sbe_pktpool;

    for (idx = 0; idx < SBETH_PKTPOOL_SIZE; idx++) {
	pkt = (sbeth_pkt_t *) ptr;
	sbeth_free_pkt(s,pkt);
	ptr += SBETH_PKTBUF_SIZE;
	}
}


/*  *********************************************************************
    *  SBETH_ALLOC_PKT(s)
    *  
    *  Allocate a packet from the free list.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   pointer to packet structure, or NULL if none available
    ********************************************************************* */

static sbeth_pkt_t *sbeth_alloc_pkt(sbeth_t *s)
{
    uintptr_t addr;
    sbeth_pkt_t *pkt = s->sbe_freelist;

    if (!pkt) return NULL;

    s->sbe_freelist = pkt->next;
    pkt->next = NULL;

    addr = (uintptr_t) (pkt+1);
    if (addr & (SBDMA_CACHESIZE-1)) {
	addr = (addr + SBDMA_CACHESIZE) & ~(SBDMA_CACHESIZE-1);
	}

    pkt->buffer = (unsigned char *) addr;
    pkt->length = SBETH_PKT_SIZE;

    return pkt;
}

/*  *********************************************************************
    *  SBETH_FREE_PKT(s,pkt)
    *  
    *  Return a packet to the free list
    *  
    *  Input parameters: 
    *  	   s - sbmac structure
    *  	   pkt - packet to return
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void sbeth_free_pkt(sbeth_t *s,sbeth_pkt_t *pkt)
{
    pkt->next = s->sbe_freelist;
    s->sbe_freelist = pkt;
}

/*  *********************************************************************
    *  SBETH_TX_CALLBACK(ifctx,chan,ctx,status,pktsize)
    *  
    *  Transmit callback routine.  This routine is invoked when a
    *  queued transmit operation completes.  In this simple driver,
    *  all we do is free the packet and try to re-fill the receive ring.
    *  
    *  Input parameters: 
    *  	   ifctx - interface context (sbeth structure)
    *  	   chan - DMA Channel
    *  	   ctx - packet context (sbeth_pkt structure)
    *  	   status - Ethernet status from descriptor
    *  	   pktsize - length of packet (unused for transmits)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_tx_callback(void *ifctx,int chan,void *ctx,
		       uint64_t status,unsigned int pktsize)
{
    sbeth_t *s = ifctx;
    sbeth_pkt_t *pkt = ctx;

    sbeth_free_pkt(s,pkt);		/* return packet to pool */

    sbeth_fillrxring(s,chan);		/* re-fill the receive ring */
}

/*  *********************************************************************
    *  SBETH_RX_CALLBACK(ifctx,chan,ctx,status,pktsize)
    *  
    *  Receive callback routine.  This routine is invoked when a
    *  buffer queued for receives is filled. In this simple driver,
    *  all we do is add the packet to a per-MAC queue for later
    *  processing, and try to put a new packet in the place of the one
    *  that was removed from the queue.
    *  
    *  Input parameters: 
    *  	   ifctx - interface context (sbeth structure)
    *  	   chan - DMA Channel
    *  	   ctx - packet context (sbeth_pkt structure)
    *  	   status - Ethernet status from descriptor
    *  	   pktsize - length of packet (unused for transmits)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void sbeth_rx_callback(void *ifctx,int chan,void *ctx,
		       uint64_t status,unsigned int pktsize)
{
    sbeth_t *s = ifctx;
    sbeth_pkt_t *pkt = ctx;
    sbeth_pkt_t *listptr;

    if (!(status & M_DMA_ETHRX_BAD)) {
	pkt->next = NULL;
	pkt->length = pktsize;

	if (s->sbe_rxqueue == NULL) {
	    s->sbe_rxqueue = pkt;
	    }
	else {
	    listptr = s->sbe_rxqueue;
	    while (listptr->next) listptr = listptr->next;
	    listptr->next = pkt;
	    }
	}
    else {
	sbeth_free_pkt(s,pkt);
	}

    sbeth_fillrxring(s,chan);
}


/*  *********************************************************************
    *  SBETH_INITCHAN(s)
    *  
    *  Initialize the Ethernet channel (program the CSRs to
    *  get the channel set up)
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_initchan(sbeth_t *s)
{
    sbeth_port_t port;
    int idx;
    uint64_t cfg,fifo,framecfg;

    /*
     * Bring the controller out of reset, and set the "must be one"
     * bits.
     */

    SBETH_WRITECSR(s->sbe_macenable,0);

    /* 
     * Set up some stuff in the control registers, but do not
     * enable the channel
     */

    cfg = M_MAC_RETRY_EN |
	M_MAC_TX_HOLD_SOP_EN | 
	V_MAC_TX_PAUSE_CNT_16K |
	V_MAC_SPEED_SEL_100MBPS |
	M_MAC_AP_STAT_EN |
	M_MAC_FAST_SYNC |
	M_MAC_SS_EN |
	0;

    fifo = V_MAC_TX_WR_THRSH(4) |	/* Must be '4' or '8' */
	   V_MAC_TX_RD_THRSH(8) |
	   V_MAC_TX_RL_THRSH(4) |
	   V_MAC_RX_PL_THRSH(4) |
	   V_MAC_RX_RD_THRSH(4) |	/* Must be '4' */
	   V_MAC_RX_PL_THRSH(4) |
    	   V_MAC_RX_RL_THRSH(8) |
	   0;


    framecfg = V_MAC_MIN_FRAMESZ_DEFAULT |
	V_MAC_MAX_FRAMESZ_DEFAULT |
	V_MAC_BACKOFF_SEL(1);


    /*
     * Clear out the hash address map 
     */
    
    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_HASH_BASE);
        for (idx = 0; idx < MAC_HASH_COUNT; idx++) {
	SBETH_WRITECSR(port,0);
	port += sizeof(uint64_t);
	}


    /*
     * Clear out the exact-match table
     */

    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_ADDR_BASE);
    for (idx = 0; idx < MAC_ADDR_COUNT; idx++) {
	SBETH_WRITECSR(port,0);
	port += sizeof(uint64_t);
	}


    /*
     * Clear out the DMA Channel mapping table registers
     */

    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_CHUP0_BASE);
    for (idx = 0; idx < MAC_CHMAP_COUNT; idx++) {
	SBETH_WRITECSR(port,0);
	port += sizeof(uint64_t);
	}

    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_CHLO0_BASE);
    for (idx = 0; idx < MAC_CHMAP_COUNT; idx++) {
	SBETH_WRITECSR(port,0);
	port += sizeof(uint64_t);
	}

    if (!s->sbe_zerormon) {
	s->sbe_zerormon =1;
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_TX_BYTES),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_COLLISIONS),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_LATE_COL),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_EX_COL),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_FCS_ERROR),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_TX_ABORT),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_TX_BAD),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_TX_GOOD),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_TX_RUNT),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_TX_OVERSIZE),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_BYTES),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_MCAST),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_BCAST),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_BAD),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_GOOD),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_RUNT),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_OVERSIZE),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_FCS_ERROR),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_LENGTH_ERROR),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_CODE_ERROR),0);
	SBETH_WRITECSR(SBETH_PORT(s->sbe_baseaddr+R_MAC_RMON_RX_ALIGN_ERROR),0);
	}


    /*
     * Configure the receive filter for no packets
     */

    SBETH_WRITECSR(s->sbe_rxfilter,0);
    SBETH_WRITECSR(s->sbe_imr,0);
    SBETH_WRITECSR(s->sbe_framecfg,framecfg);
    SBETH_WRITECSR(s->sbe_fifocfg,fifo);
    SBETH_WRITECSR(s->sbe_maccfg,cfg);


}

/*  *********************************************************************
    *  SBETH_INITCTX(s,mac)
    *  
    *  Initialize an Ethernet context structure - this is called
    *  once per MAC on the 1250.
    *  
    *  Input parameters: 
    *  	   s - sbeth context structure
    *  	   mac - number of this MAC (0,1,2)
    *      ifctx - interface context (reference saved by driver)
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */
static int sbeth_initctx(sbeth_t *s,unsigned long baseaddr,void *ifctx)
{

    /* 
     * Start with all zeroes 
     */
    memset(s,0,sizeof(sbeth_t));

    /* 
     * Identify ourselves 
     */

    s->sbe_baseaddr = baseaddr;
    s->sbe_ifctx   = ifctx;

    s->sbe_minrxring = 8;

    /*
     * Set default hardware address.  This is in case there is *no* environment.
     */

    s->sbe_hwaddr[0] = 0x40;
    s->sbe_hwaddr[1] = 0x00;
    s->sbe_hwaddr[2] = (s->sbe_baseaddr >> 24) & 0xFF;
    s->sbe_hwaddr[3] = (s->sbe_baseaddr >> 16) & 0xFF;
    s->sbe_hwaddr[4] = (s->sbe_baseaddr >>  8) & 0xFF;
    s->sbe_hwaddr[5] = (s->sbe_baseaddr >>  0) & 0xFF;

    /* 
     * figure out the addresses of some ports 
     */
    s->sbe_macenable = SBETH_PORT(s->sbe_baseaddr + R_MAC_ENABLE);
    s->sbe_maccfg    = SBETH_PORT(s->sbe_baseaddr + R_MAC_CFG);
    s->sbe_fifocfg   = SBETH_PORT(s->sbe_baseaddr + R_MAC_THRSH_CFG);
    s->sbe_framecfg  = SBETH_PORT(s->sbe_baseaddr + R_MAC_FRAMECFG);
    s->sbe_rxfilter  = SBETH_PORT(s->sbe_baseaddr + R_MAC_ADFILTER_CFG);

    s->sbe_isr = SBETH_PORT(s->sbe_baseaddr + R_MAC_STATUS);
    s->sbe_imr = SBETH_PORT(s->sbe_baseaddr + R_MAC_INT_MASK);

    s->sbe_mdio = SBETH_PORT(s->sbe_baseaddr + R_MAC_MDIO);

    /*
     * Initialize the DMA channels.  
     */

    sbdma_initctx(s,&(s->sbe_txdma[0]),0,DMA_TX,SBETH_MAX_TXDESCR,sbeth_tx_callback);
    sbdma_initctx(s,&(s->sbe_rxdma[0]),0,DMA_RX,SBETH_MAX_RXDESCR,sbeth_rx_callback);
#if (SBETH_DMA_CHANNELS == 2)
    sbdma_initctx(s,&(s->sbe_txdma[1]),1,DMA_TX,SBETH_MAX_TXDESCR,sbeth_tx_callback);
    sbdma_initctx(s,&(s->sbe_rxdma[1]),1,DMA_RX,SBETH_MAX_RXDESCR,sbeth_rx_callback);
#endif

    /*
     * initialize free list
     */

    s->sbe_freelist = NULL;
    s->sbe_rxqueue = NULL;

    s->sbe_pktpool = KMALLOC(SBETH_PKTBUF_SIZE*SBETH_PKTPOOL_SIZE,
			     SBDMA_CACHESIZE);


    /*
     * Set values for the PHY so that when we poll the phy status
     * we'll notice that it has changed.
     */

    s->sbe_phy_oldbmsr   = 0xFFFFFFFF;
    s->sbe_phy_oldbmcr   = 0xFFFFFFFF;
    s->sbe_phy_oldanlpar = 0xFFFFFFFF;
    s->sbe_phy_oldk1stsr = 0xFFFFFFFF;

    /*
     * initial state is OFF
     */

    s->sbe_state = sbeth_state_off;

    return 0;
}


/*  *********************************************************************
    *  SBETH_START(s)
    *  
    *  Start packet processing on this MAC.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_start(sbeth_t *s)
{
    uint64_t ctl;	

    sbdma_initchan(s,&(s->sbe_txdma[0]));
    sbdma_initchan(s,&(s->sbe_rxdma[0]));
#if (SBETH_DMA_CHANNELS == 2)
    sbdma_initchan(s,&(s->sbe_txdma[1]));
    sbdma_initchan(s,&(s->sbe_rxdma[1]));
#endif
    sbeth_initchan(s);

    sbeth_setspeed(s,s->sbe_speed);
    sbeth_set_duplex(s,s->sbe_duplex,s->sbe_fc);

    SBETH_WRITECSR(s->sbe_rxfilter,0);

    ctl = SBETH_READCSR(s->sbe_macenable);

    ctl |= M_MAC_RXDMA_EN0 |
   	   M_MAC_TXDMA_EN0 |
#if (SBETH_DMA_CHANNELS == 2)
	   M_MAC_TXDMA_EN1 |
	   M_MAC_RXDMA_EN1 |
#endif
	   M_MAC_RX_ENABLE |
	   M_MAC_TX_ENABLE |
	0;

    sbeth_initfreelist(s);

    SBETH_WRITECSR(s->sbe_macenable,ctl);

    sbeth_setaddr(s,s->sbe_hwaddr);

#ifdef _SB1250_PASS1_WORKAROUNDS_
    /* Must set the Ethernet address to zero in pass1 */
    do {
	sbeth_port_t port;
	port = SBETH_PORT(s->sbe_baseaddr + R_MAC_ETHERNET_ADDR);
	SBETH_WRITECSR(port,0);
	} while (0);
#endif

    sbeth_fillrxring(s,0);

    SBETH_WRITECSR(s->sbe_rxfilter,M_MAC_UCAST_EN | M_MAC_BCAST_EN |
	V_MAC_IPHDR_OFFSET(15) |
		   /* M_MAC_ALLPKT_EN |*/  /* uncomment for promisc mode */
	0
	);

    s->sbe_state = sbeth_state_on;

}

/*  *********************************************************************
    *  SBETH_STOP(s)
    *  
    *  Stop packet processing on this MAC.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_stop(sbeth_t *s)
{
    uint64_t ctl;
    int mac_mdio_genc;

    SBETH_WRITECSR(s->sbe_rxfilter,0);

    ctl = SBETH_READCSR(s->sbe_macenable);

    ctl &= ~(M_MAC_RXDMA_EN0 | M_MAC_TXDMA_EN0 | M_MAC_RXDMA_EN1 | M_MAC_TXDMA_EN1 |
	M_MAC_RX_ENABLE | M_MAC_TX_ENABLE);

    SBETH_WRITECSR(s->sbe_macenable,ctl);

    /*
     * The genc bit on the MAC MDIO register needs to be preserved through reset.
     * Read the MAC MDIO register and mask out genc bit.
     */
    mac_mdio_genc = SBETH_READCSR(s->sbe_mdio) & M_MAC_GENC;
    
    ctl |= M_MAC_PORT_RESET;

    SBETH_WRITECSR(s->sbe_macenable,ctl);

    /* Write back value of genc bit */
    SBETH_WRITECSR(s->sbe_mdio,mac_mdio_genc);

    s->sbe_state = sbeth_state_off;

    sbdma_reset(&(s->sbe_txdma[0]));
    sbdma_reset(&(s->sbe_rxdma[0]));


}


/*  *********************************************************************
    *  SBETH_SETADDR(s,addr)
    *  
    *  Set the ethernet address for the specified MAC
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   addr - Ethernet address
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_setaddr(sbeth_t *s,uint8_t *addr)
{
    sbeth_port_t port;
    uint64_t regval = 0;
    int idx;

    /*
     * Pack the bytes into the register, with the first byte transmitted
     * in the lowest-order 8 bits of the register.
     */

    for (idx = 0; idx < 6; idx++) {
	regval |= (((uint64_t) (*addr)) << (idx*8));
	addr++;
	}


    /*
     * Write to the port.
     */

    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_ETHERNET_ADDR);
    SBETH_WRITECSR(port,regval);

    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_ADDR_BASE);
    SBETH_WRITECSR(port,regval);

}

/*  *********************************************************************
    *  SBETH_SETSPEED(s,speed)
    *  
    *  Configure LAN speed for the specified MAC
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   speed - speed to set MAC to (see sbeth_speed_t enum)
    *  	   
    *  Return value:
    *  	   1 if successful
    *      0 indicates invalid parameters
    ********************************************************************* */

static int sbeth_setspeed(sbeth_t *s,sbeth_speed_t speed)
{
    uint64_t cfg;
    uint64_t framecfg;

    /*
     * Read current register values 
     */

    cfg = SBETH_READCSR(s->sbe_maccfg);
    framecfg = SBETH_READCSR(s->sbe_framecfg);

    /*
     * Mask out the stuff we want to change
     */

    cfg &= ~(M_MAC_BURST_EN | M_MAC_SPEED_SEL);
    framecfg &= ~(M_MAC_IFG_RX | M_MAC_IFG_TX | M_MAC_IFG_THRSH |
		  M_MAC_SLOT_SIZE);

    /*
     * Now add in the new bits
     */

    switch (speed) {
	case sbeth_speed_10:
	    framecfg |= V_MAC_IFG_RX_10 |
		V_MAC_IFG_TX_10 |
		K_MAC_IFG_THRSH_10 |
		V_MAC_SLOT_SIZE_10;
	    cfg |= V_MAC_SPEED_SEL_10MBPS;
	    break;

	case sbeth_speed_100:
	    framecfg |= V_MAC_IFG_RX_100 |
		V_MAC_IFG_TX_100 |
		V_MAC_IFG_THRSH_100 |
		V_MAC_SLOT_SIZE_100;
	    cfg |= V_MAC_SPEED_SEL_100MBPS ;
	    break;

	case sbeth_speed_1000:
	    framecfg |= V_MAC_IFG_RX_1000 |
		V_MAC_IFG_TX_1000 |
		V_MAC_IFG_THRSH_1000 |
		V_MAC_SLOT_SIZE_1000;
	    cfg |= V_MAC_SPEED_SEL_1000MBPS | M_MAC_BURST_EN;
	    break;

	default:
	    return 0;
	}

    /*
     * Send the bits back to the hardware 
     */

    SBETH_WRITECSR(s->sbe_framecfg,framecfg);
    SBETH_WRITECSR(s->sbe_maccfg,cfg);

    return 1;

}

/*  *********************************************************************
    *  SBETH_SET_DUPLEX(s,duplex,fc)
    *  
    *  Set Ethernet duplex and flow control options for this MAC
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   duplex - duplex setting (see sbeth_duplex_t)
    *  	   fc - flow control setting (see sbeth_fc_t)
    *  	   
    *  Return value:
    *  	   1 if ok
    *  	   0 if an invalid parameter combination was specified
    ********************************************************************* */

static int sbeth_set_duplex(sbeth_t *s,sbeth_duplex_t duplex,sbeth_fc_t fc)
{
    uint64_t cfg;

    /*
     * Read current register values 
     */

    cfg = SBETH_READCSR(s->sbe_maccfg);

    /*
     * Mask off the stuff we're about to change
     */

    cfg &= ~(M_MAC_FC_SEL | M_MAC_FC_CMD | M_MAC_HDX_EN);


    switch (duplex) {
	case sbeth_duplex_half:
	    switch (fc) {
		case sbeth_fc_disabled:
		    cfg |= M_MAC_HDX_EN | V_MAC_FC_CMD_DISABLED;
		    break;

		case sbeth_fc_collision:
		    cfg |= M_MAC_HDX_EN | V_MAC_FC_CMD_ENABLED;
		    break;

		case sbeth_fc_carrier:
		    cfg |= M_MAC_HDX_EN | V_MAC_FC_CMD_ENAB_FALSECARR;
		    break;

		case sbeth_fc_frame:		/* not valid in half duplex */
		default:			/* invalid selection */
		    return 0;
		}
	    break;

	case sbeth_duplex_full:
	    switch (fc) {
		case sbeth_fc_disabled:
		    cfg |= V_MAC_FC_CMD_DISABLED;
		    break;

		case sbeth_fc_frame:
		    cfg |=  V_MAC_FC_CMD_ENABLED;
		    break;

		case sbeth_fc_collision:	/* not valid in full duplex */
		case sbeth_fc_carrier:		/* not valid in full duplex */
		    /* fall through */					   
		default:
		    return 0;
		}
	    break;
	}

    /*
     * Send the bits back to the hardware 
     */

    SBETH_WRITECSR(s->sbe_maccfg,cfg);

    return 1;
}


/*  *********************************************************************
    *  SBETH_TRANSMIT(s,pkt,len,arg)
    *  
    *  Transmits a packet.
    *  
    *  Input parameters: 
    *  	   s - mac to tramsmit on
    *      chan - DMA Channel number (0 or 1)
    *  	   pkt,len - buffer and length
    *  	   arg - arg for callback
    *  	   
    *  Return value:
    *  	   1 if packet was queued
    *  	   0 if packet was not queued
    ********************************************************************* */
static int sbeth_transmit(sbeth_t *s,int chan,unsigned char *pkt,int length,void *arg)
{
    return sbdma_addbuffer(&(s->sbe_txdma[chan]),pkt,length,arg);
}

/*  *********************************************************************
    *  SBETH_ADDRCVBUF(s,pkt,len,arg)
    *  
    *  Add a receive buffer to the ring
    *  
    *  Input parameters: 
    *  	   s - mac to add rx buffer to
    *      chan - DMA Channel number (0 or 1)
    *  	   pkt,len - buffer and length
    *  	   arg - arg for callback
    *  	   
    *  Return value:
    *  	   1 if packet was queued
    *  	   0 if packet was not queued
    ********************************************************************* */
static int sbeth_addrcvbuf(sbeth_t *s,int chan,unsigned char *pkt,int length,void *arg)
{
    return sbdma_addbuffer(&(s->sbe_rxdma[chan]),pkt,length,arg);
}


/*  *********************************************************************
    *  SBETH_FILLRXRING(s,chan)
    *  
    *  Make sure there are at least "sbe_minrxring" packets on the
    *  receive ring for this device.
    *  
    *  Input parameters: 
    *  	   s - mac structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sbeth_fillrxring(sbeth_t *s,int chan)
{
    sbeth_pkt_t *pkt;

    while (s->sbe_rxdma[chan].sbdma_onring < s->sbe_minrxring) {
	pkt = sbeth_alloc_pkt(s);
	if (!pkt) break;
	if (!sbeth_addrcvbuf(s,chan,pkt->buffer,pkt->length,pkt)) {
	    sbeth_free_pkt(s,pkt);
	    break;
	    }
	}
}



/*  *********************************************************************
    *  SBETH_ISR()
    *  
    *  Interrupt handler for MAC interrupts
    *  
    *  Input parameters: 
    *  	   MAC structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void sbeth_isr(sbeth_t *s)
{
    uint64_t isr;
    for (;;) {


	/*
	 * Read the ISR (this clears the bits in the real register)
	 */

	isr = SBETH_READCSR(s->sbe_isr);

	if (isr == 0)  {
		break;
	} 
	
	/*
	 * for now, don't bother asking why we were interrupted,
	 * just process the descriptors in any event.
	 */


	/*
	 * Transmits on channel 0
	 */

	if (isr & (M_MAC_INT_CHANNEL << S_MAC_TX_CH0)) {
	    sbdma_procbuffers(&(s->sbe_txdma[0]),s->sbe_txdma[0].sbdma_upcall);
	}

#if (SBETH_DMA_CHANNELS == 2)
	/*	
	 * Transmits on channel 1
	 */

	if (isr & (M_MAC_INT_CHANNEL << S_MAC_TX_CH1)) {
	    sbdma_procbuffers(&(s->sbe_txdma[1]),s->sbe_txdma[1].sbdma_upcall);
	    }
#endif

	/*
	 * Receives on channel 0
	 */

	if (isr & (M_MAC_INT_CHANNEL << S_MAC_RX_CH0)) {
	    sbdma_procbuffers(&(s->sbe_rxdma[0]),s->sbe_rxdma[0].sbdma_upcall);
	    }

#if (SBETH_DMA_CHANNELS == 2)
	/*
	 * Receives on channel 1
	 */

	if (isr & (M_MAC_INT_CHANNEL << S_MAC_RX_CH1)) {
            sbdma_procbuffers(&(s->sbe_rxdma[1]),s->sbe_rxdma[1].sbdma_upcall);
	    }
#endif
	}

}


/*  *********************************************************************
    *  SBETH_PARSE_XDIGIT(str)
    *  
    *  Parse a hex digit, returning its value
    *  
    *  Input parameters: 
    *  	   str - character
    *  	   
    *  Return value:
    *  	   hex value, or -1 if invalid
    ********************************************************************* */

static int sbeth_parse_xdigit(char str)
{
    int digit;

    if ((str >= '0') && (str <= '9')) digit = str - '0';
    else if ((str >= 'a') && (str <= 'f')) digit = str - 'a' + 10;
    else if ((str >= 'A') && (str <= 'F')) digit = str - 'A' + 10;
    else return -1;

    return digit;
}

/*  *********************************************************************
    *  SBETH_PARSE_HWADDR(str,hwaddr)
    *  
    *  Convert a string in the form xx:xx:xx:xx:xx:xx into a 6-byte
    *  Ethernet address.
    *  
    *  Input parameters: 
    *  	   str - string
    *  	   hwaddr - pointer to hardware address
    *  	   
    *  Return value:
    *  	   0 if ok, else -1
    ********************************************************************* */

static int sbeth_parse_hwaddr(char *str,uint8_t *hwaddr)
{
    int digit1,digit2;
    int idx = 6;

    while (*str && (idx > 0)) {
	digit1 = sbeth_parse_xdigit(*str);
	if (digit1 < 0) return -1;
	str++;
	if (!*str) return -1;

	if ((*str == ':') || (*str == '-')) {
	    digit2 = digit1;
	    digit1 = 0;
	    }
	else {
	    digit2 = sbeth_parse_xdigit(*str);
	    if (digit2 < 0) return -1;
	    str++;
	    }

	*hwaddr++ = (digit1 << 4) | digit2;
	idx--;

	if (*str == '-') str++;
	if (*str == ':') str++;
	}
    return 0;
}

/*  *********************************************************************
    *  SBETH_MII_FINDPHY(s)
    *  
    *  Find the first available PHY.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void sbeth_mii_findphy(sbeth_t *s)
{
    int phy;
    uint16_t bmsr;

    for (phy = 0; phy < 31; phy++) {
	bmsr = sbeth_mii_read(s,phy,MII_BMSR);
	if (bmsr != 0) {
	    s->sbe_phyaddr = phy;
	    return;
	    }
	}

    s->sbe_phyaddr = 0;
}

/*  *********************************************************************
    *  SBETH_MII_POLL(s)
    *  
    *  Ask the PHY what is going on, and configure speed appropriately.
    *  For the moment, we only support automatic configuration.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *      noisy - display console messages
    *  	   
    *  Return value:
    *  	   1 if something has changed and we should restart the channel
    *	   0 if nothing has changed.
    ********************************************************************* */
static int sbeth_mii_poll(sbeth_t *s,int noisy)
{
    uint16_t bmsr,bmcr,k1stsr,anlpar;
    int chg;
    char buffer[100];
    char *p = buffer;
    char *devname;

    /* Read the mode status and mode control registers. */
    bmsr = sbeth_mii_read(s,s->sbe_phyaddr,MII_BMSR);
    bmcr = sbeth_mii_read(s,s->sbe_phyaddr,MII_BMCR);

    /* get the link partner status */
    anlpar = sbeth_mii_read(s,s->sbe_phyaddr,MII_ANLPAR);

    /* if supported, read the 1000baseT register */
    if (bmsr & BMSR_1000BT_XSR) {
	k1stsr = sbeth_mii_read(s,s->sbe_phyaddr,MII_K1STSR);
	}
    else {
	k1stsr = 0;
	}

    chg = 0;

    if ((s->sbe_phy_oldbmsr != bmsr) ||
	(s->sbe_phy_oldbmcr != bmcr) ||
	(s->sbe_phy_oldanlpar != anlpar) ||
	(s->sbe_phy_oldk1stsr != k1stsr)) {
	s->sbe_phy_oldbmsr = bmsr;
	s->sbe_phy_oldbmcr = bmcr;
	s->sbe_phy_oldanlpar = anlpar;
	s->sbe_phy_oldk1stsr = k1stsr;
	chg = 1;
	}

    if (chg == 0) return 0;

    p += xsprintf(p,"Link speed: ");

    if (k1stsr & K1STSR_LP1KFD) {
	s->sbe_speed = sbeth_speed_1000;
	s->sbe_duplex = sbeth_duplex_full;
	s->sbe_fc = sbeth_fc_frame;
	s->sbe_linkstat = ETHER_SPEED_1000FDX;
	p += xsprintf(p,"1000BaseT FDX");
	}
    else if (k1stsr & K1STSR_LP1KHD) {
	s->sbe_speed = sbeth_speed_1000;
	s->sbe_duplex = sbeth_duplex_half;
	s->sbe_fc = sbeth_fc_disabled;
	s->sbe_linkstat = ETHER_SPEED_1000HDX;
	p += xsprintf(p,"1000BaseT HDX");
	}
    else if (anlpar & ANLPAR_TXFD) {
	s->sbe_speed = sbeth_speed_100;
	s->sbe_duplex = sbeth_duplex_full;
	s->sbe_fc = (anlpar & ANLPAR_PAUSE) ? sbeth_fc_frame : sbeth_fc_disabled;
	s->sbe_linkstat = ETHER_SPEED_100FDX;
	p += xsprintf(p,"100BaseT FDX");
	}
    else if (anlpar & ANLPAR_TXHD) {
	s->sbe_speed = sbeth_speed_100;
	s->sbe_duplex = sbeth_duplex_half;
	s->sbe_fc = sbeth_fc_disabled;
	s->sbe_linkstat = ETHER_SPEED_100HDX;
	p += xsprintf(p,"100BaseT HDX");
	}
    else if (anlpar & ANLPAR_10FD) {
	s->sbe_speed = sbeth_speed_10;
	s->sbe_duplex = sbeth_duplex_full;
	s->sbe_fc = sbeth_fc_frame;
	s->sbe_linkstat = ETHER_SPEED_10FDX;
	p += xsprintf(p,"10BaseT FDX");
	}
    else if (anlpar & ANLPAR_10HD) {
	s->sbe_speed = sbeth_speed_10;
	s->sbe_duplex = sbeth_duplex_half;
	s->sbe_fc = sbeth_fc_collision;
	s->sbe_linkstat = ETHER_SPEED_10HDX;
	p += xsprintf(p,"10BaseT HDX");
	}
    else {
	s->sbe_linkstat = ETHER_SPEED_UNKNOWN;
	p += xsprintf(p,"Unknown");
	}

#if defined(_BCM91120C_DIAG_CFG_) || defined(_BCM91125C_DIAG_CFG_) || \
	defined(_CSWARM_DIAG_CFG_) || defined(_CSWARM_DIAG3E_CFG_) || \
	defined(_PTSWARM_DIAG_CFG_) || defined(_PTSWARM_CFG_)
    noisy = 0;
#endif

    if (noisy) {
	devname = s->sbe_devctx ? cfe_device_name(s->sbe_devctx) : "eth?";
	console_log("%s: %s",devname,buffer);
	}

    return 1;
}


/*  *********************************************************************
    *  SBETH_MII_SONG_AND_DANCE(s)
    *  
    *  The CSWARM boards leave the PHYs in JTAG mode.  The sequence
    *  below turns off JTAG mode and puts the PHYs back
    *  into their regular reset state.  This is only used with the BCM5411
    *  
    *  Input parameters: 
    *  	   s - sbeth
    *  	   
    *  Return value:
    *  	   TRUE if we were on a 5411
    ********************************************************************* */
static int sbeth_mii_song_and_dance(sbeth_t *s)
{
    int phyid1,phyid2;

    phyid1 = sbeth_mii_read(s,1,MII_PHYIDR1);
    phyid2 = sbeth_mii_read(s,1,MII_PHYIDR2);

    /* Check for the 5411.  Don't do this unless it is a 5411. */

    if ((phyid1 == 0x0020) && ((phyid2 & 0xFFF0) == 0x6070)) {
	/* It's a BCM5411 */
	/* clear ext loopback */
	sbeth_mii_write(s,1,MII_AUXCTL,0x0420);

	/* clear swap rx MDIX/TXHalfOut bits */
	sbeth_mii_write(s,1,MII_AUXCTL,0x0004);

	/* set up 10/100 advertisement */
	sbeth_mii_write(s,1,MII_ANAR,0x01E1);

	/* set up 1000 advertisement */
	sbeth_mii_write(s,1,MII_K1CTL,0x0300);

	/* set autonegotiate bit and restart autoneg */
	sbeth_mii_write(s,1,MII_BMCR,0x1340);
	return 1;
	}

    /* Check for the 5421.  Don't do this unless it is a 5421. */

    if ((phyid1 == 0x0020) && ((phyid2 & 0xFFF0) == 0x60E0)) {
	/* It's a BCM5421 */
    
	/* 
	 * Make sure that the part is in GMII, not SGMII.  
	 * This was a problem with 5421 A0 silicon
	 * the FDX pin
	 */
	sbeth_mii_write(s,1,0x18,0x0392);
    
	return 1;
	}


    return 0;
}


/*  *********************************************************************
    *  Declarations for CFE Device Driver Interface routines
    ********************************************************************* */

static int sb1250_ether_open(cfe_devctx_t *ctx);
static int sb1250_ether_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int sb1250_ether_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int sb1250_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int sb1250_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int sb1250_ether_close(cfe_devctx_t *ctx);
static void sb1250_ether_poll(cfe_devctx_t *ctx,int64_t ticks);
static void sb1250_ether_reset(void *softc);

/*  *********************************************************************
    *  CFE Device Driver dispatch structure
    ********************************************************************* */

const static cfe_devdisp_t sb1250_ether_dispatch = {
    sb1250_ether_open,
    sb1250_ether_read,
    sb1250_ether_inpstat,
    sb1250_ether_write,
    sb1250_ether_ioctl,
    sb1250_ether_close,
    sb1250_ether_poll,
    sb1250_ether_reset
};

/*  *********************************************************************
    *  CFE Device Driver descriptor
    ********************************************************************* */

const cfe_driver_t sb1250_ether = {
    "SB1250 Ethernet",
    "eth",
    CFE_DEV_NETWORK,
    &sb1250_ether_dispatch,
    sb1250_ether_probe
};


/*  *********************************************************************
    *  SB1250_ETHER_PROBE(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Probe and install an Ethernet device driver.  This routine
    *  creates a context structure and attaches to the specified
    *  MAC device.
    *  
    *  Input parameters: 
    *  	   drv - driver descriptor
    *  	   probe_a - base address of MAC to probe
    *  	   probe_b - not used
    *  	   probe_ptr - string pointer to hardware address for this
    *  	               MAC, in the form xx:xx:xx:xx:xx:xx
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_ether_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr)
{
    sbeth_t *softc;
    char descr[100];

    softc = (sbeth_t *) KMALLOC(sizeof(sbeth_t),0);

    if (softc) {
	sbeth_initctx(softc,probe_a,softc);
	if (probe_ptr) {
	    sbeth_parse_hwaddr((char *) probe_ptr,softc->sbe_hwaddr);
	    }
	xsprintf(descr,"%s at 0x%X (%a)",drv->drv_description,probe_a,
		 softc->sbe_hwaddr);
	sbeth_mii_song_and_dance(softc);
	cfe_attach(drv,softc,NULL,descr);
	sbeth_setaddr(softc,softc->sbe_hwaddr);
	}
}


/*  *********************************************************************
    *  SB1250_ETHER_READENV(ctx)
    *  
    *  Read the environment variable that corresponds to this
    *  interface to pick up the hardware address.  Note that the way
    *  we do this is somewhat slimey.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_ether_readenv(cfe_devctx_t *ctx)
{
    sbeth_t *softc = ctx->dev_softc;
    char envbuf[100];
    char *hwaddr;

    /*
     * Gross - we should *not* be reaching into these data
     * structures like this!
     */

    xsprintf(envbuf,"%s_HWADDR",cfe_device_name(ctx));
    strupr(envbuf);
    
    hwaddr = env_getenv(envbuf);

    if (hwaddr) {
	sbeth_parse_hwaddr(hwaddr,softc->sbe_hwaddr);
	}
   
}

/*  *********************************************************************
    *  SB1250_MII_DUMP(s)
    *  
    *  Dump out the MII registers
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


/*  *********************************************************************
    *  SB1250_ETHER_OPEN(ctx)
    *  
    *  Open the Ethernet device.  The MAC is reset, initialized, and
    *  prepared to receive and send packets.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int sb1250_ether_open(cfe_devctx_t *ctx)
{
    sbeth_t *softc = ctx->dev_softc;

    softc->sbe_devctx = ctx;

    sbeth_stop(softc);

    sbeth_mii_findphy(softc);

    /*
     * Note: The Phy can take several seconds to become ready!
     * This gross code pounds on the phy until  it says it is
     * ready, but it still takes 2 more seconds after this
     * before the link is usable.  We're better off letting the
     * dhcp/arp retries do the right thing here.
     */

    sbeth_mii_poll(softc,TRUE);

    sb1250_ether_readenv(ctx);

    TIMER_SET(softc->sbe_linkstat_timer,SBETH_MIIPOLL_TIMER);
    softc->sbe_autospeed = TRUE;
    softc->fifo_mode = FALSE;

    sbeth_start(softc);

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_READ(ctx,buffer)
    *  
    *  Read a packet from the Ethernet device.  If no packets are
    *  available, the read will succeed but return 0 bytes.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      buffer - pointer to buffer descriptor.  
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int sb1250_ether_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    sbeth_t *softc = ctx->dev_softc;
    sbeth_pkt_t *pkt;
    int blen;

    if (softc->sbe_state != sbeth_state_on) return -1;

    sbeth_isr(softc);

    if (softc->sbe_rxqueue == NULL) {
	buffer->buf_retlen = 0;
	return 0;
	}

    pkt = softc->sbe_rxqueue;
    softc->sbe_rxqueue = pkt->next;
    pkt->next = NULL;

    blen = buffer->buf_length;
    if (blen > pkt->length) blen = pkt->length;

    memcpy(buffer->buf_ptr,pkt->buffer,blen);
    buffer->buf_retlen = blen;

    sbeth_free_pkt(softc,pkt);
    sbeth_fillrxring(softc,0);
    sbeth_isr(softc);

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_INPSTAT(ctx,inpstat)
    *  
    *  Check for received packets on the Ethernet device
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      inpstat - pointer to input status structure
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */
static int sb1250_ether_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    sbeth_t *softc = ctx->dev_softc;

    if (softc->sbe_state != sbeth_state_on) return -1;

    sbeth_isr(softc);

    inpstat->inp_status = (softc->sbe_rxqueue == NULL) ? 0 : 1;

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_WRITE(ctx,buffer)
    *  
    *  Write a packet to the Ethernet device.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      buffer - pointer to buffer descriptor.  
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */
static int sb1250_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    sbeth_t *softc = ctx->dev_softc;
    sbeth_pkt_t *pkt;
    int blen;

    if (softc->sbe_state != sbeth_state_on) return -1;


    if (!softc->fifo_mode) {
	/* Only do a speed check if not packet fifo mode*/
	if (softc->sbe_linkstat == ETHER_SPEED_UNKNOWN) {
	    sbeth_mii_poll(softc,1);
	    if (softc->sbe_linkstat == ETHER_SPEED_UNKNOWN) return -1;
	    }
	}

    pkt = sbeth_alloc_pkt(softc);
    if (!pkt) return CFE_ERR_NOMEM;

    blen = buffer->buf_length;
    if (blen > pkt->length) blen = pkt->length;

    memcpy(pkt->buffer,buffer->buf_ptr,blen);
    pkt->length = blen;


    sbeth_isr(softc);

    if (sbeth_transmit(softc,0,pkt->buffer,pkt->length,pkt) != 1) {
	sbeth_free_pkt(softc,pkt);
	return CFE_ERR_IOERR;
	}

    sbeth_isr(softc);

    buffer->buf_retlen = blen;

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_IOCTL_LOOPBACK(s,loopback)
    *  
    *  Set loopback modes
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   loopback - loopback modes
    *  	   
    *  Return value:
    *  	   0 if ok, else error
    ********************************************************************* */

static int sb1250_ether_ioctl_loopback(sbeth_t *s,int loopback)
{
    unsigned int miireg;
    uint64_t regval;

    switch (loopback) {
	case ETHER_LOOPBACK_OFF:
	    miireg = sbeth_mii_read(s,s->sbe_phyaddr,MII_BMCR);
	    if (miireg & BMCR_LOOPBACK) {
		miireg &= ~BMCR_LOOPBACK;
		miireg |= BMCR_RESTARTAN;
		sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,miireg);
		}
	    regval = SBETH_READCSR(s->sbe_maccfg);
	    if (regval & M_MAC_LOOPBACK_SEL) {
		regval &= ~M_MAC_LOOPBACK_SEL;
		SBETH_WRITECSR(s->sbe_maccfg,regval);
		}
	    break;

	case ETHER_LOOPBACK_INT:
	    regval = SBETH_READCSR(s->sbe_maccfg);
	    regval |= M_MAC_LOOPBACK_SEL;
	    SBETH_WRITECSR(s->sbe_maccfg,regval);
	    break;

	case ETHER_LOOPBACK_EXT:
	    regval = SBETH_READCSR(s->sbe_maccfg);
	    if (regval & M_MAC_LOOPBACK_SEL) {
		regval &= ~M_MAC_LOOPBACK_SEL;
		SBETH_WRITECSR(s->sbe_maccfg,regval);
		}
	    miireg = sbeth_mii_read(s,s->sbe_phyaddr,MII_BMCR);
	    miireg |= BMCR_LOOPBACK;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,miireg);
	    break;
	}

    s->sbe_loopback = loopback;

    return 0;
}


/*  *********************************************************************
    *  SB1250_ETHER_IOCTL_SPEED(s,speed)
    *  
    *  Set speed forcibly via the IOCTL command
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   speed - speed IOCTL setting
    *  	   
    *  Return value:
    *  	   0 if ok, else error
    ********************************************************************* */

static int sb1250_ether_ioctl_speed(sbeth_t *s,int speed)
{
    switch (speed) {
	case ETHER_SPEED_AUTO:
	    s->sbe_autospeed = TRUE;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			    BMCR_ANENABLE|BMCR_RESTARTAN|BMCR_DUPLEX|BMCR_SPEED1);
	    TIMER_SET(s->sbe_linkstat_timer,100);
	    break;

	case ETHER_SPEED_10HDX:
	    s->sbe_autospeed = FALSE;
	    s->sbe_speed = sbeth_speed_10;
	    s->sbe_duplex = sbeth_duplex_half;
	    s->sbe_fc = sbeth_fc_disabled;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			BMCR_SPEED10);
	    break;

	case ETHER_SPEED_10FDX:
	    s->sbe_autospeed = FALSE;
	    s->sbe_speed = sbeth_speed_10;
	    s->sbe_duplex = sbeth_duplex_full;
	    s->sbe_fc = sbeth_fc_frame;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			BMCR_SPEED10|BMCR_DUPLEX);
	    break;

	case ETHER_SPEED_100HDX:
	    s->sbe_autospeed = FALSE;
	    s->sbe_speed = sbeth_speed_100;
	    s->sbe_duplex = sbeth_duplex_half;
	    s->sbe_fc = sbeth_fc_disabled;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			BMCR_SPEED100);
	    break;

	case ETHER_SPEED_100FDX:
	    s->sbe_autospeed = FALSE;
	    s->sbe_speed = sbeth_speed_100;
	    s->sbe_duplex = sbeth_duplex_full;
	    s->sbe_fc = sbeth_fc_frame;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			BMCR_SPEED100|BMCR_DUPLEX);
	    break;

	case ETHER_SPEED_1000HDX:
	    s->sbe_autospeed = FALSE;
	    s->sbe_speed = sbeth_speed_1000;
	    s->sbe_duplex = sbeth_duplex_half;
	    s->sbe_fc = sbeth_fc_disabled;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			BMCR_SPEED1000);
	    break;
	
	case ETHER_SPEED_1000FDX:
	    s->sbe_autospeed = FALSE;
	    s->sbe_speed = sbeth_speed_1000;
	    s->sbe_duplex = sbeth_duplex_full;
	    s->sbe_fc = sbeth_fc_frame;
	    sbeth_mii_write(s,s->sbe_phyaddr,MII_BMCR,
			BMCR_SPEED1000|BMCR_DUPLEX);
	    break;

	default:
	    return -1;
	}

    sbeth_stop(s);
    sbeth_start(s);

    s->sbe_curspeed = speed;
    if (speed != ETHER_SPEED_AUTO) s->sbe_linkstat = speed;

    return 0;
}
/*  *********************************************************************
    *  SB1250_ETHER_IOCTL_PACKETFIFO(s,mode)
    *  
    *  Swtich to a packet fifo mode.
    *  
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   mode - 8 or 16 bit packet fifo mode.
    *  	   
    *  Return value:
    *  	   0 if ok, else error 
    ********************************************************************* */
static int sb1250_ether_ioctl_packetfifo(sbeth_t *s, int mode)
{
    uint64_t cfg;
    uint64_t enb;
    uint64_t frame;

    cfg = SBETH_READCSR(s->sbe_maccfg);
    switch (mode) {
	case ETHER_FIFO_8:
	    cfg &= ~(M_MAC_BYPASS_SEL | M_MAC_AP_STAT_EN | M_MAC_SPEED_SEL | M_MAC_BURST_EN);
	    cfg |= M_MAC_BYPASS_SEL | V_MAC_SPEED_SEL_1000MBPS | M_MAC_BURST_EN;
 
	    /* disable rx/tx ethernet macs and  enable rx/tx fifo engines */
	    enb = SBETH_READCSR(s->sbe_macenable);    
	    enb &= ~(M_MAC_RX_ENABLE | M_MAC_TX_ENABLE);
	    enb |= M_MAC_BYP_RX_ENABLE |
		M_MAC_BYP_TX_ENABLE |
		0;
	    SBETH_WRITECSR(s->sbe_macenable,enb);

	    /* accept all packets */
	    SBETH_WRITECSR(s->sbe_rxfilter, M_MAC_ALLPKT_EN | 0);

	    /* set min_frame_size to 9 bytes */
	    frame = SBETH_READCSR(s->sbe_framecfg);
	    frame |= V_MAC_MIN_FRAMESZ_FIFO;
	    SBETH_WRITECSR(s->sbe_framecfg,frame);

	    s->fifo_mode = TRUE;

	    break;

	case ETHER_FIFO_16:
	    cfg &= ~(M_MAC_BYPASS_SEL | M_MAC_BYPASS_16 | M_MAC_AP_STAT_EN | M_MAC_SPEED_SEL | M_MAC_BURST_EN);
	    cfg |= M_MAC_BYPASS_SEL | M_MAC_BYPASS_16 | V_MAC_SPEED_SEL_1000MBPS | M_MAC_BURST_EN;
 
	    /* disable rx/tx ethernet macs and  enable rx/tx fifo engines */
	    enb = SBETH_READCSR(s->sbe_macenable);    
	    enb &= ~(M_MAC_RX_ENABLE | M_MAC_TX_ENABLE);
	    enb |= M_MAC_BYP_RX_ENABLE |
		M_MAC_BYP_TX_ENABLE |
		0;
	    SBETH_WRITECSR(s->sbe_macenable,enb);

	    /* accept all packets */
	    SBETH_WRITECSR(s->sbe_rxfilter, M_MAC_ALLPKT_EN | 0);

	    /* set min_frame_size to 9 bytes */
	    frame = SBETH_READCSR(s->sbe_framecfg);
	    frame |= V_MAC_MIN_FRAMESZ_FIFO;
	    SBETH_WRITECSR(s->sbe_framecfg,frame);

	    s->fifo_mode = TRUE;

	    break;

	case ETHER_ETHER:
	    cfg &= ~(M_MAC_BYPASS_SEL | M_MAC_BYPASS_16 | M_MAC_AP_STAT_EN );
	    cfg |= M_MAC_AP_STAT_EN;
	    break;

	default:      
	    return -1;
	}

    SBETH_WRITECSR(s->sbe_maccfg,cfg);

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_IOCTL_STROBESIGNAL
    *  
    *  Set the strobe signal that are used on both transmit and receive 
    *  interfaces in packet fifo mode.
    *  
    *  Input parameters: 
    *  	   s - sbeth structure
    *  	   mode - GMII style, Encoded, SOP flagged, or EOP flagged mode.
    *  	   
    *  Return value:
    * 	   0 if ok, else error
    ********************************************************************* */
static int sb1250_ether_ioctl_strobesignal(sbeth_t *s, int mode)
{
    uint64_t cfg;

    cfg = SBETH_READCSR(s->sbe_maccfg);

    switch (mode) {
	case ETHER_STROBE_GMII:
	    cfg &= ~(M_MAC_BYPASS_CFG);
	    cfg |= V_MAC_BYPASS_CFG(K_MAC_BYPASS_GMII); 
	    break;

	case ETHER_STROBE_ENCODED:
	    cfg &= ~(M_MAC_BYPASS_CFG);
	    cfg |= V_MAC_BYPASS_CFG(K_MAC_BYPASS_ENCODED); 
	    break;

	case ETHER_STROBE_SOP:	/* not valid in 16-bit fifo mode */
	    cfg &= ~(M_MAC_BYPASS_CFG);
	    cfg |= V_MAC_BYPASS_CFG(K_MAC_BYPASS_SOP); 
	    break;

	case ETHER_STROBE_EOP: /* not valid in 16-bit fifo mode */
	    cfg &= ~(M_MAC_BYPASS_CFG);
	    cfg |= V_MAC_BYPASS_CFG(K_MAC_BYPASS_EOP); 
	    break;

	default:      
	    return -1;
	}

    SBETH_WRITECSR(s->sbe_maccfg,cfg);

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_IOCTL(ctx,buffer)
    *  
    *  Do device-specific I/O control operations for the device
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      buffer - pointer to buffer descriptor.  
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */
static int sb1250_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    sbeth_t *softc = ctx->dev_softc;
    int *mode;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_ETHER_GETHWADDR:
	    memcpy(buffer->buf_ptr,softc->sbe_hwaddr,sizeof(softc->sbe_hwaddr));
	    break;

	case IOCTL_ETHER_SETHWADDR:
	    memcpy(softc->sbe_hwaddr,buffer->buf_ptr,sizeof(softc->sbe_hwaddr));
	    sbeth_setaddr(softc,softc->sbe_hwaddr);
#ifdef _SB1250_PASS1_WORKAROUNDS_
	    SBETH_WRITECSR(SBETH_PORT(softc->sbe_baseaddr + R_MAC_ETHERNET_ADDR),0);
#endif
	    break;

	case IOCTL_ETHER_GETSPEED:
	    mode = (int *) buffer->buf_ptr;
	    *mode = softc->sbe_curspeed;
	    break;

	case IOCTL_ETHER_SETSPEED:
	    mode = (int *) buffer->buf_ptr;
	    return sb1250_ether_ioctl_speed(softc,*mode);
	    break;

	case IOCTL_ETHER_GETLINK:
	    mode = (int *) buffer->buf_ptr;
	    *mode = softc->sbe_linkstat;
	    break;

	case IOCTL_ETHER_GETLOOPBACK:
	    mode = (int *) buffer->buf_ptr;
	    *mode = softc->sbe_loopback;
	    break;

	case IOCTL_ETHER_SETLOOPBACK:
	    mode = (int *) buffer->buf_ptr;
	    return sb1250_ether_ioctl_loopback(softc,*mode);
	    break;

        case IOCTL_ETHER_SETPACKETFIFO:
	    mode = (int *) buffer->buf_ptr;
	    return sb1250_ether_ioctl_packetfifo(softc,*mode);
	    break;
	    
	case IOCTL_ETHER_SETSTROBESIG:
	    mode = (int *) buffer->buf_ptr;
	    return sb1250_ether_ioctl_strobesignal(softc,*mode);
	    break;
	    
	default:
	    return -1;
	}

    return 0;
}

/*  *********************************************************************
    *  SB1250_ETHER_CLOSE(ctx)
    *  
    *  Close the Ethernet device.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */
static int sb1250_ether_close(cfe_devctx_t *ctx)
{
    sbeth_t *softc = ctx->dev_softc;

    sbeth_stop(softc);

    /* Reprogram the default hardware address in case someone mucked with it */
    sbeth_setaddr(softc,softc->sbe_hwaddr);

    return 0;
}


/*  *********************************************************************
    *  SB1250_ETHER_POLL(ctx,ticks)
    *  
    *  Check for changes in the PHY, so we can track speed changes.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      ticks- current time in ticks
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_ether_poll(cfe_devctx_t *ctx,int64_t ticks)
{
    sbeth_t *softc = ctx->dev_softc;
    int chg;

    if (TIMER_RUNNING(softc->sbe_linkstat_timer) &&
	TIMER_EXPIRED(softc->sbe_linkstat_timer)) {
	if (softc->sbe_autospeed) {
	    chg = sbeth_mii_poll(softc,TRUE);
	    if (chg) {
		if (softc->sbe_state == sbeth_state_on) {
		    TIMER_CLEAR(softc->sbe_linkstat_timer);
		    sbeth_stop(softc);
		    sbeth_start(softc);
		    }
		}
	    }
	TIMER_SET(softc->sbe_linkstat_timer,SBETH_MIIPOLL_TIMER);
	}
    
}

/*  *********************************************************************
    *  SB1250_ETHER_RESET(softc)
    *  
    *  This routine is called when CFE is restarted after a 
    *  program exits.  We can clean up pending I/Os here.
    *  
    *  Input parameters: 
    *  	   softc - pointer to sbmac_t
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_ether_reset(void *softc)
{
    sbeth_t *s = (sbeth_t *) softc;
    sbeth_port_t port;
    uint64_t regval = 0;
    int idx;
    uint8_t *addr;

    /*
     * Turn off the Ethernet interface.
     */

    SBETH_WRITECSR(s->sbe_macenable,0);

    /*
     * Reset the address.
     * Pack the bytes into the register, with the first byte transmitted
     * in the lowest-order 8 bits of the register.
     */

    addr = s->sbe_hwaddr;
    for (idx = 0; idx < 6; idx++) {
	regval |= (((uint64_t) (*addr)) << (idx*8));
	addr++;
	}


    /*
     * Write to the port.
     */

    port = SBETH_PORT(s->sbe_baseaddr + R_MAC_ETHERNET_ADDR);
    SBETH_WRITECSR(port,regval);

}
