/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM5700/Tigon3 (10/100/1000 EthernetMAC) driver	File: dev_bcm5700.c
    *  
    *  Author:  Ed Satterthwaite
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

#ifndef _SB_MAKE64
#define _SB_MAKE64(x) ((uint64_t)(x))
#endif
#ifndef _SB_MAKEMASK1
#define _SB_MAKEMASK1(n) (_SB_MAKE64(1) << _SB_MAKE64(n))
#endif

#include "lib_types.h"
#include "lib_physio.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "lib_queue.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "cfe_error.h"
#include "cfe_irq.h"

#include "pcivar.h"
#include "pcireg.h"

#include "bcm5700.h"
#include "mii.h"

#include "bsp_config.h"

#include "proto/ethernet.h"
#include "bcmdevs.h"
#include "bcmutils.h"
#include "bcmnvram.h"
#include "hndsoc.h"
#include "siutils.h"
#include "hndgige.h"
#include "bcmrobo.h"

static int sigige = -1;

/* This is a driver for the Broadcom 570x ("Tigon 3") 10/100/1000 MAC.
   Currently, the 5700, 5701, 5703C, 5704C and 5705 have been tested.
   Only 10/100/1000 BASE-T PHYs are supported; variants with SerDes
   PHYs are not supported.

   Reference:
     Host Programmer Interface Specification for the BCM570X Family
       of Highly-Integrated Media Access Controllers, 570X-PG106-R.
     Broadcom Corp., 16215 Alton Parkway, Irvine CA, 09/27/02

   This driver takes advantage of DMA coherence in systems that
   support it (e.g., SB1250).  For systems without coherent DMA (e.g.,
   BCM47xx SOCs), descriptor and packet buffer memory is explicitly
   flushed.

   The driver prefers "preserve bit lanes" mode for big-endian
   systems that provide the option, but it can use "preserve byte
   lanes" as well.

   Note that the 5705 does not fully map all address ranges.  Per
   the manual, reads and writes of the unmapped regions are permitted
   and do not fault; however, it apparently has some poisoned registers,
   at least in early revs, that should not be touched.  See the
   conditionals in the code. */

/* PIOSWAP controls whether word-swapping takes place for transactions
   in which the 570x is the target device.  In theory, either value
   should work (with access macros adjusted as below) and it should be
   set to be consistent with the settings for 570x as initiator.
   Empirically, however, some combinations only work with no swap.
   For big-endian systems:

                          SWAP=0    SWAP=1
   5700     32 PCI          OK        OK
   5700     64 Sturgeon     OK        OK
   5701-32  32 PCI          OK        OK
   5701-32  64 Sturgeon     OK        OK
   5701-32  64 Golem        OK        OK
   5701-64  64 Sturgeon     OK        OK
   5701-64  64 Golem        OK       FAIL
   5705     32 PCI          OK        OK
   5705     64 Sturgeon    (OK)*     FAIL
   5705     64 Golem        OK        OK

   For little-endian systems, only SWAP=1 appears to work.

   * PCI status/interrupt ordering problem under load.  */
     
#if	__MIPSEL
#define	PIOSWAP	1     
#else
#define PIOSWAP 0
#endif

#ifndef T3_DEBUG
#define T3_DEBUG 0
#endif

#ifndef T3_BRINGUP
#define T3_BRINGUP 0
#endif


/* Broadcom recommends using PHY interrupts instead of autopolling,
   but I haven't made it work yet. */
#define T3_AUTOPOLL 1

/* Set IPOLL to drive processing through the interrupt dispatcher.
   Set XPOLL to drive processing by an external polling agent.  One
   must be set; setting both is ok. */

#ifndef IPOLL
#define IPOLL 0
#endif
#ifndef XPOLL
#define XPOLL 1
#endif

#define ENET_ADDR_LEN	6		/* size of an ethernet address */
#define MIN_ETHER_PACK  64              /* min size of a packet */
#define MAX_ETHER_PACK  1518		/* max size of a packet */
#define VLAN_TAG_LEN    4               /* VLAN type plus tag */
#define CRC_SIZE	4		/* size of CRC field */

/* Packet buffers.  For the Tigon 3, packet buffer alignment is
   arbitrary and can be to any byte boundary.  We would like it
   aligned to a cache line boundary for performance, although there is
   a trade-off with IP/TCP header alignment.  Jumbo frames are not
   currently supported.  */

#define ETH_PKTBUF_LEN      (((MAX_ETHER_PACK+31)/32)*32)

#if __long64
typedef struct eth_pkt_s {
    queue_t next;			/* 16 */
    uint8_t *buffer;			/*  8 */
    uint32_t flags;			/*  4 */
    int32_t length;			/*  4 */
    uint8_t data[ETH_PKTBUF_LEN];
} eth_pkt_t;
#else
typedef struct eth_pkt_s {
    queue_t next;			/*  8 */
    uint8_t *buffer;			/*  4 */
    uint32_t flags;			/*  4 */
    int32_t length;			/*  4 */
    uint32_t unused[3];			/* 12 */
    uint8_t data[ETH_PKTBUF_LEN];
} eth_pkt_t;
#endif

#define CACHE_ALIGN       32
#define ETH_PKTBUF_LINES  ((sizeof(eth_pkt_t) + (CACHE_ALIGN-1))/CACHE_ALIGN)
#define ETH_PKTBUF_SIZE   (ETH_PKTBUF_LINES*CACHE_ALIGN)
#define ETH_PKTBUF_OFFSET (offsetof(eth_pkt_t, data))

#define ETH_PKT_BASE(data) ((eth_pkt_t *)((data) - ETH_PKTBUF_OFFSET))

static void
show_packet(char c, eth_pkt_t *pkt)
{
    int i;
    int n = (pkt->length < 32 ? pkt->length : 32);

    xprintf("%c[%4d]:", c, (int)pkt->length);
    for (i = 0; i < n; i++) {
	if (i % 4 == 0)
	    xprintf(" ");
	xprintf("%02x", pkt->buffer[i]);
	}
    xprintf("\n");
}


static void t3_ether_probe(cfe_driver_t *drv,
			   unsigned long probe_a, unsigned long probe_b, 
			   void *probe_ptr);



/* Chip documentation numbers the rings with 1-origin.  */

#define RI(n)                 ((n)-1)

/* BCM570x Ring Sizes (no external memory).  Pages 97-98 */

#define TXP_MAX_RINGS         16
#define TXP_INTERNAL_RINGS    4
#define TXP_RING_ENTRIES      512

#define RXP_STD_ENTRIES       512

#define RXR_MAX_RINGS         16
#define RXR_RING_ENTRIES      1024

#define RXR_MAX_RINGS_05      1
#define RXR_RING_ENTRIES_05   512


/* BCM570x Send Buffer Descriptors as a struct.  Pages 100-101 */

typedef struct t3_snd_bd_s {
    uint32_t  bufptr_hi;
    uint32_t  bufptr_lo;
#ifdef __MIPSEB
    uint16_t  length;
    uint16_t  flags;
    uint16_t  pad;
    uint16_t  vlan_tag;
#elif __MIPSEL
    uint16_t  flags;
    uint16_t  length;
    uint16_t  vlan_tag;
    uint16_t  pad;
#else
#error "bcm5700: endian not set"
#endif
} t3_snd_bd_t;

#define SND_BD_SIZE           16

#define TX_FLAG_TCP_CKSUM     0x0001
#define TX_FLAG_IP_CKSUM      0x0002
#define TX_FLAG_PACKET_END    0x0004
#define TX_FLAG_IP_FRAG       0x0008
#define TX_FLAG_IP_FRAG_END   0x0010
#define TX_FLAG_VLAN_TAG      0x0040
#define TX_FLAG_COAL_NOW      0x0080
#define TX_FLAG_CPU_PRE_DMA   0x0100
#define TX_FLAG_CPU_POST_DMA  0x0200
#define TX_FLAG_ADD_SRC       0x1000
#define TX_FLAG_SRC_ADDR_SEL  0x6000
#define TX_FLAG_NO_CRC        0x8000

/* BCM570x Receive Buffer Descriptors as a struct.  Pages 105-107 */

typedef struct t3_rcv_bd_s {
    uint32_t  bufptr_hi;
    uint32_t  bufptr_lo;
#ifdef __MIPSEB
    uint16_t  index;
    uint16_t  length;
    uint16_t  type;
    uint16_t  flags;
    uint16_t  ip_cksum;
    uint16_t  tcp_cksum;
    uint16_t  error_flag;
    uint16_t  vlan_tag;
#elif __MIPSEL
    uint16_t  length;
    uint16_t  index;
    uint16_t  flags;
    uint16_t  type;
    uint16_t  tcp_cksum;
    uint16_t  ip_cksum;
    uint16_t  vlan_tag;
    uint16_t  error_flag;
#else
#error "bcm5700: endian not set"
#endif
    uint32_t  pad;
    uint32_t  opaque;
} t3_rcv_bd_t;

#define RCV_BD_SIZE           32

#define RX_FLAG_PACKET_END    0x0004
#define RX_FLAG_JUMBO_RING    0x0020
#define RX_FLAG_VLAN_TAG      0x0040
#define RX_FLAG_ERROR         0x0400
#define RX_FLAG_MINI_RING     0x0800
#define RX_FLAG_IP_CKSUM      0x1000
#define RX_FLAG_TCP_CKSUM     0x2000
#define RX_FLAG_IS_TCP        0x4000

#define RX_ERR_BAD_CRC        0x0001
#define RX_ERR_COLL_DETECT    0x0002
#define RX_ERR_LINK_LOST      0x0004
#define RX_ERR_PHY_DECODE     0x0008
#define RX_ERR_DRIBBLE        0x0010
#define RX_ERR_MAC_ABORT      0x0020
#define RX_ERR_SHORT_PKT      0x0040
#define RX_ERR_TRUNC_NO_RES   0x0080
#define RX_ERR_GIANT_PKT      0x0100

/* BCM570x Status Block format as a struct (not BCM5705).  Pages 110-111. */

typedef struct t3_status_s {
    uint32_t status;
    uint32_t tag;
#ifdef __MIPSEB
    uint16_t rxc_std_index;
    uint16_t rxc_jumbo_index;
    uint16_t reserved2;
    uint16_t rxc_mini_index;
    struct {
	uint16_t send_c;
	uint16_t return_p;
    } index [16];
#elif __MIPSEL
    uint16_t rxc_jumbo_index;
    uint16_t rxc_std_index;
    uint16_t rxc_mini_index;
    uint16_t reserved2;
    struct {
	uint16_t return_p;
	uint16_t send_c;
    } index [16];
#else
#error "bcm5700: endian not set"
#endif
} t3_status_t;

#define M_STATUS_UPDATED        0x00000001
#define M_STATUS_LINKCHNG       0x00000002
#define M_STATUS_ERROR          0x00000004

/* BCM570x Statistics Block format as a struct.  Pages 112-120 */

typedef struct t3_stats_s {
    uint64_t stats[L_MAC_STATS/sizeof(uint64_t)];
} t3_stats_t;

/* Encoded status transfer block size (32, 64 or 80 bytes.  Page 412 */

#define STATUS_BLOCK_SIZE(rings) \
         ((rings) <= 4  ? K_HCM_SBSIZE_32 : \
          (rings) <= 12 ? K_HCM_SBSIZE_64 : \
          K_HCM_SBSIZE_80) 

/* End of 570X defined data structures */

/* The maximum supported BD ring index (QOS) for tranmit or receive. */

#define MAX_RI                 1


typedef enum {
    eth_state_uninit,
    eth_state_off,
    eth_state_on, 
} eth_state_t;

typedef struct t3_ether_s {
    /* status block */
    volatile t3_status_t *status;  /* should be cache-aligned */

    /* PCI access information */
    uint32_t  regbase;
    uint32_t  membase;
    uint8_t   irq;
    pcitag_t  tag;		   /* tag for configuration registers */

    uint8_t   hwaddr[6];
    uint16_t  device;              /* chip device code */
    uint8_t   revision;            /* chip revision */
    uint16_t  asic_revision;       /* mask revision */

    eth_state_t state;             /* current state */
    uint32_t intmask;              /* interrupt mask */

    int linkspeed;		   /* encodings from cfe_ioctl */

    /* packet lists */
    queue_t freelist;
    uint8_t *pktpool;
    queue_t rxqueue;

    /* rings */
    /* For now, support only the standard Rx Producer Ring */
    t3_rcv_bd_t *rxp_std;          /* Standard Rx Producer Ring */
    uint32_t  rxp_std_index;
    uint32_t  prev_rxp_std_index;

   /* For now, support only 1 priority */
    uint32_t  rxr_entries;
    t3_rcv_bd_t *rxr_1;            /* Rx Return Ring 1 */
    uint32_t  rxr_1_index;
    t3_snd_bd_t *txp_1;            /* Send Ring 1 */
    uint32_t  txp_1_index;
    uint32_t  txc_1_index;

    cfe_devctx_t *devctx;

    /* PHY access */
    int      phy_addr;
    uint16_t phy_status;
    uint16_t phy_ability;
    uint16_t phy_xability;
    uint32_t phy_vendor;
    uint16_t phy_device;

    /* MII polling control */
    int      phy_change;
    int      mii_polling;

    /* statistics block */
    volatile t3_stats_t *stats;    /* should be cache-aligned */

    /* additional driver statistics */
    uint32_t rx_interrupts;
    uint32_t tx_interrupts;
    uint32_t bogus_interrupts;

    /* SB specific fields */
    si_t     *sih;
    uint32_t siidx;
    uint32_t flags;
#define T3_RGMII_MODE 	0x1
#define T3_SB_CORE	0x2
#define T3_NO_PHY	0x4
} t3_ether_t;


/* Address mapping macros */

#define PCI_TO_PTR(a)  (PHYS_TO_K1(a))
#define PTR_TO_PCI(x)  (K1_TO_PHYS((uint32_t)x))


/* Chip access macros */

/* These macros attempt to be compatible with match-bits mode,
   which may put the data and byte masks into the wrong 32-bit word
   for 64-bit accesses.  See the comment above on PIOSWAP.
   Externally mastered DMA (control and data) uses match-bits and does
   specify word-swaps when operating big endian.  */

/* Most registers are 32 bits wide and are accessed by 32-bit
   transactions.  The mailbox registers and on-chip RAM are 64-bits
   wide but are generally accessed by 32-bit transactions.
   Furthermore, the documentation is ambiguous about which 32-bits of
   the mailbox is significant.  To localize the potential confusions,
   we define macros for the 3 different cases.  */

#define READCSR(sc,csr)       phys_read32((sc)->regbase + (csr))
#define WRITECSR(sc,csr,val)  phys_write32((sc)->regbase + (csr), (val))

#if PIOSWAP
#define READMBOX(sc,csr)      phys_read32((sc)->regbase+((csr)^4))
#define WRITEMBOX(sc,csr,val) phys_write32((sc)->regbase+((csr)^4), (val))

#define READMEM(sc,csr)       phys_read32((sc)->membase+(csr))
#define WRITEMEM(sc,csr,val)  phys_write32((sc)->membase+(csr), (val))

#else
#define READMBOX(sc,csr)      phys_read32((sc)->regbase+(csr))
#define WRITEMBOX(sc,csr,val) phys_write32((sc)->regbase+(csr), (val))

#define READMEM(sc,csr)       phys_read32((sc)->membase+((csr) ^ 4))
#define WRITEMEM(sc,csr,val)  phys_write32((sc)->membase+((csr) ^ 4), (val))

#endif


/* Entry to and exit from critical sections (currently relative to
   interrupts only, not SMP) */

#if CFG_INTERRUPTS
#define CS_ENTER(sc) cfe_disable_irq(sc->irq)
#define CS_EXIT(sc)  cfe_enable_irq(sc->irq)
#else
#define CS_ENTER(sc) ((void)0)
#define CS_EXIT(sc)  ((void)0)
#endif


static void
dumpseq(t3_ether_t *sc, int start, int next)
{
    int offset, i, j;
    int columns = 4;
    int lines = (((next - start)/4 + 1) + 3)/columns;
    int step = lines*4;

    offset = start;
    for (i = 0; i < lines; i++) {
	xprintf("\nCSR");
	for (j = 0; j < columns; j++) {
	    if (offset + j*step < next)
		xprintf(" %04X: %08lX ",
			offset+j*step, READCSR(sc, offset+j*step));
	    }
	offset += 4;
	}
    xprintf("\n");
}

static void
dumpcsrs(t3_ether_t *sc, const char *legend)
{
    xprintf("%s:\n", legend);

    /* Some device-specific PCI configuration registers */
    xprintf("-----PCI-----");
    dumpseq(sc, 0x68, 0x78);

    /* Some general control registers */
    xprintf("---General---");
    dumpseq(sc, 0x6800, 0x6810);

    xprintf("-------------\n");
}


/* Memory allocation */

static void *
kmalloc_uncached( unsigned int size, unsigned int align )
{
    void *   ptr;
	
    if ((ptr = KMALLOC(size, align)) == NULL)
        return NULL;

    cfe_flushcache(CFE_CACHE_FLUSH_D);

	return (void *)UNCADDR(PHYSADDR((uint32_t)ptr));
}

static void
kfree_uncached( void * ptr )
{
	KFREE((void *)KERNADDR(PHYSADDR((uint32_t)ptr)));
}


/* Packet management */

#define ETH_PKTPOOL_SIZE  64
#define MIN_RXP_STD_BDS   32


static eth_pkt_t *
eth_alloc_pkt(t3_ether_t *sc)
{
    eth_pkt_t *pkt;

    CS_ENTER(sc);
    pkt = (eth_pkt_t *) q_deqnext(&sc->freelist);
    CS_EXIT(sc);
    if (!pkt) return NULL;

    pkt->buffer = pkt->data;
    pkt->length = ETH_PKTBUF_LEN;
    pkt->flags = 0;

    return pkt;
}


static void
eth_free_pkt(t3_ether_t *sc, eth_pkt_t *pkt)
{
    CS_ENTER(sc);
    q_enqueue(&sc->freelist, &pkt->next);
    CS_EXIT(sc);
}

static void
eth_initfreelist(t3_ether_t *sc)
{
    int idx;
    uint8_t *ptr;
    eth_pkt_t *pkt;

    q_init(&sc->freelist);

    ptr = sc->pktpool;
    for (idx = 0; idx < ETH_PKTPOOL_SIZE; idx++) {
	pkt = (eth_pkt_t *) ptr;
	eth_free_pkt(sc, pkt);
	ptr += ETH_PKTBUF_SIZE;
	}
}


/* Utilities */

static const char *
t3_devname(t3_ether_t *sc)
{
    return (sc->devctx != NULL ? cfe_device_name(sc->devctx) : "eth?");
}


/* CRCs */

#define IEEE_CRC32_POLY    0xEDB88320UL    /* CRC-32 Poly -- either endian */

uint32_t eth_crc32(const uint8_t *databuf, unsigned int datalen);
/*static*/ uint32_t
eth_crc32(const uint8_t *databuf, unsigned int datalen) 
{       
    unsigned int idx, bit, data;
    uint32_t crc;

    crc = 0xFFFFFFFFUL;
    for (idx = 0; idx < datalen; idx++)
	for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1)
	    crc = (crc >> 1) ^ (((crc ^ data) & 1) ? IEEE_CRC32_POLY : 0);
    return crc;
}


/* Descriptor ring management */

static int
t3_add_rcvbuf(t3_ether_t *sc, eth_pkt_t *pkt)
{
    t3_rcv_bd_t *rxp;

    rxp = &(sc->rxp_std[sc->rxp_std_index]);
    rxp->bufptr_lo = PTR_TO_PCI(pkt->buffer);
    rxp->length = ETH_PKTBUF_LEN;
    sc->rxp_std_index++;
    if (sc->rxp_std_index == RXP_STD_ENTRIES)
	sc->rxp_std_index = 0;
    return 0;
}

static void
t3_fillrxring(t3_ether_t *sc)
{
    eth_pkt_t *pkt;
    unsigned rxp_ci, rxp_onring;

    rxp_ci = sc->status->rxc_std_index;  /* Get a snapshot */

    if (sc->rxp_std_index >= rxp_ci)
	rxp_onring = sc->rxp_std_index - rxp_ci;
    else
	rxp_onring = (sc->rxp_std_index + RXP_STD_ENTRIES) - rxp_ci;

    while (rxp_onring < MIN_RXP_STD_BDS) {
	pkt = eth_alloc_pkt(sc);
	if (pkt == NULL) {
	    /* could not allocate a buffer */
	    break;
	    }

	/*
	 * Ensure that the packet memory is flushed out of the data cache
	 * before posting it to receive an incoming packet.
	 */
	cfe_flushcache(CFE_CACHE_FLUSH_D);

	if (t3_add_rcvbuf(sc, pkt) != 0) {
	    /* could not add buffer to ring */
	    eth_free_pkt(sc, pkt);
	    break;
	    }
	rxp_onring++;
	}
}

static void
t3_rx_callback(t3_ether_t *sc, eth_pkt_t *pkt)
{
    if (T3_DEBUG) show_packet('>', pkt);   /* debug */

    CS_ENTER(sc);
    q_enqueue(&sc->rxqueue, &pkt->next);
    CS_EXIT(sc);
}

static void
t3_procrxring(t3_ether_t *sc)
{
    eth_pkt_t   *pkt;
    t3_rcv_bd_t *rxc;
    volatile t3_status_t *status = sc->status;

    rxc = &(sc->rxr_1[sc->rxr_1_index]);
    do {
	pkt = ETH_PKT_BASE(PCI_TO_PTR(rxc->bufptr_lo));
	pkt->length = rxc->length;
	if ((rxc->flags & RX_FLAG_ERROR) == 0)
	    t3_rx_callback(sc, pkt);
	else {
#if T3_BRINGUP
	    xprintf("%s: rx error %04X\n", t3_devname(sc), rxc->error_flag);
#endif
	    eth_free_pkt(sc, pkt);   /* Could optimize */
	    }
	sc->rxr_1_index++;
	rxc++;
	if (sc->rxr_1_index == sc->rxr_entries) {
	    sc->rxr_1_index = 0;
	    rxc = &(sc->rxr_1[0]);
	    }
	} while (status->index[RI(1)].return_p != sc->rxr_1_index);

    /* Update the return ring */
    WRITEMBOX(sc, R_RCV_BD_RTN_CI(1), sc->rxr_1_index);

    /* Refill the producer ring */
    t3_fillrxring(sc);
}


static int
t3_transmit(t3_ether_t *sc, eth_pkt_t *pkt)
{
    t3_snd_bd_t *txp;

    if (T3_DEBUG) show_packet('<', pkt);   /* debug */


    txp = &(sc->txp_1[sc->txp_1_index]);
    txp->bufptr_hi = 0;
    txp->bufptr_lo = PTR_TO_PCI(pkt->buffer);
    txp->length = pkt->length;
    txp->flags = TX_FLAG_PACKET_END;

    sc->txp_1_index++;
    if (sc->txp_1_index == TXP_RING_ENTRIES)
	sc->txp_1_index = 0;

    WRITEMBOX(sc, R_SND_BD_PI(1), sc->txp_1_index);

    return 0;
}


static void
t3_proctxring(t3_ether_t *sc)
{
    eth_pkt_t   *pkt;
    t3_snd_bd_t *txc;
    volatile t3_status_t *status = sc->status;

    txc = &(sc->txp_1[sc->txc_1_index]);
    do {
	pkt = ETH_PKT_BASE(PCI_TO_PTR(txc->bufptr_lo));
	eth_free_pkt(sc, pkt);
	sc->txc_1_index++;
	txc++;
	if (sc->txc_1_index == TXP_RING_ENTRIES) {
	    sc->txc_1_index = 0;
	    txc = &(sc->txp_1[0]);
	    }
	} while (status->index[RI(1)].send_c != sc->txc_1_index);
}


static void
t3_initrings(t3_ether_t *sc)
{
    int  i;
    t3_rcv_bd_t *rxp;
    volatile t3_status_t *status = sc->status;

    /* Clear all Producer BDs */
    rxp = &(sc->rxp_std[0]);
    for (i = 0; i < RXP_STD_ENTRIES; i++) {
        rxp->bufptr_hi = rxp->bufptr_lo = 0;
	rxp->length = 0;
	rxp->index = i;
	rxp->flags = 0;
	rxp->type = 0;
	rxp->ip_cksum = rxp->tcp_cksum = 0;
	rxp++;
	}

    /* Init the ring pointers */

    sc->rxp_std_index = 0;  status->rxc_std_index = 0;
    sc->rxr_1_index = 0;    status->index[RI(1)].return_p = 0;
    sc->txp_1_index = 0;    status->index[RI(1)].send_c = 0;

    /* Allocate some initial buffers for the Producer BD ring */
    sc->prev_rxp_std_index = 0;
    t3_fillrxring(sc);

    /* Nothing consumed yet */
    sc->txc_1_index = 0;
}

static void
t3_init(t3_ether_t *sc)
{
    /* Allocate buffer pool */
    sc->pktpool = KMALLOC(ETH_PKTPOOL_SIZE*ETH_PKTBUF_SIZE, CACHE_ALIGN);
    eth_initfreelist(sc);
    q_init(&sc->rxqueue);
    t3_initrings(sc);
}

static void
t3_reinit(t3_ether_t *sc)
{
    eth_initfreelist(sc);
    q_init(&sc->rxqueue);

    t3_initrings(sc);
}


#ifdef __MIPSEB
/* Byte swap utilities. */

#define SWAP4(x) \
    ((((x) & 0x00FF) << 24) | \
     (((x) & 0xFF00) << 8)  | \
     (((x) >> 8) & 0xFF00)  | \
     (((x) >> 24) & 0x00FF))

static uint32_t
swap4(uint32_t x)
{
    uint32_t t;

    t = ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8);
    return (t >> 16) | ((t & 0xFFFF) << 16);
}
#endif /* __MIPSEB */


/* EEPROM access functions (BCM5700 and BCM5701 version) */

/* The 570x chips support multiple access methods.  We use "Auto Access",
   which requires that
     Miscellaneous_Local_Control.Auto_SEEPROM_Access be set,
     Serial_EEprom.Address.HalfClock be programmed for <= 400 Hz.
   (both done by initialization code) */

#define EP_MAX_RETRIES  500
#define EP_DEVICE_ID    0x00           /* default ATMEL device ID */

static void
eeprom_access_init(t3_ether_t *sc)
{
  uint32_t mlctl;

  if (sc->flags & T3_SB_CORE)
	  return;

  WRITECSR(sc, R_EEPROM_ADDR, M_EPADDR_RESET | V_EPADDR_HPERIOD(0x60));

  mlctl = READCSR(sc, R_MISC_LOCAL_CTRL);
  mlctl |= M_MLCTL_EPAUTOACCESS;
  WRITECSR(sc, R_MISC_LOCAL_CTRL, mlctl);
}


static uint32_t
eeprom_read_word(t3_ether_t *sc, unsigned int offset)
{
    /* Assumes that SEEPROM is already set up for auto access. */
    uint32_t epaddr, epdata;
    volatile uint32_t temp;
    int i;

    if (sc->flags & T3_SB_CORE)
	    return 0xffffffff;

    epaddr = READCSR(sc, R_EEPROM_ADDR);
    epaddr &= M_EPADDR_HPERIOD;
    epaddr |= (V_EPADDR_ADDR(offset) | V_EPADDR_DEVID(EP_DEVICE_ID)
	       | M_EPADDR_RW | M_EPADDR_START | M_EPADDR_COMPLETE);
    WRITECSR(sc, R_EEPROM_ADDR, epaddr);
    temp = READCSR(sc, R_EEPROM_ADDR);   /* push */

    for (i = 0; i < EP_MAX_RETRIES; i++) {
        temp = READCSR(sc, R_EEPROM_ADDR);
	if ((temp & M_EPADDR_COMPLETE) != 0)
	    break;
	cfe_usleep(10);
    }
    if (i == EP_MAX_RETRIES)
	xprintf("%s: eeprom_read_word: no SEEPROM response @ %x\n",
		t3_devname(sc), offset);

    epdata = READCSR(sc, R_EEPROM_DATA);   /* little endian */
#ifdef __MIPSEB
    return swap4(epdata);
#else
    return epdata;
#endif
}

static int
eeprom_read_range(t3_ether_t *sc, unsigned int offset, unsigned int len,
		  uint32_t buf[])
{
    int index;

    offset &= ~3;  len &= ~3;     /* 4-byte words only */
    index = 0;
    
    while (len > 0) {
	buf[index++] = eeprom_read_word(sc, offset);
	offset += 4;  len -= 4;
	}

    return index;
}

static void
eeprom_dump_range(const char *label,
		  uint32_t buf[], unsigned int offset, unsigned int len)
{
    int index;

    xprintf("EEPROM: %s", label);

    offset &= ~3;  len &= ~3;     /* 4-byte words only */
    index = 0;

    for (index = 0; len > 0; index++) {
	if (index % 8 == 0)
	    xprintf("\n %04x: ", offset);
	xprintf(" %08lx", buf[offset/4]);
	offset += 4;  len -= 4;
	}
    xprintf("\n");
}


/* MII access functions.  */

/* BCM5401 device specific registers */

#define MII_ISR         0x1A    /* Interrupt Status Register */
#define MII_IMR         0x1B    /* Interrupt Mask Register */

#define M_INT_LINKCHNG  0x0002


/* The 570x chips support multiple access methods.  We use "Auto
   Access", which requires that MDI_Control_Register.MDI_Select be
   clear (done by initialization code) */

#define MII_MAX_RETRIES 5000

static void
mii_access_init(t3_ether_t *sc)
{
    WRITECSR(sc, R_MDI_CTRL, 0);                    /* here for now */
#if !T3_AUTOPOLL
    WRITECSR(sc, R_MI_MODE, V_MIMODE_CLKCNT(0x1F));  /* max divider */
#endif
}


static uint16_t
mii_read_register(t3_ether_t *sc, int phy, int index)
{
    uint32_t mode;
    uint32_t comm, val;
    int   i;

    mode = READCSR(sc, R_MI_MODE);

    comm = (V_MICOMM_CMD_RD | V_MICOMM_PHY(phy) | V_MICOMM_REG(index)
	    | M_MICOMM_BUSY);
    WRITECSR(sc, R_MI_COMM, comm);

    for (i = 0; i < MII_MAX_RETRIES; i++) {
	val = READCSR(sc, R_MI_COMM);
	if ((val & M_MICOMM_BUSY) == 0)
	    break;
	}	
    if (i == MII_MAX_RETRIES)
	xprintf("%s: mii_read_register: MII always busy\n", t3_devname(sc));


    return G_MICOMM_DATA(val);
}

/* Register reads occasionally return spurious 0's.  Verify a zero by
   doing a second read, or spinning when a zero is "impossible".  */
static uint16_t
mii_read_register_v(t3_ether_t *sc, int phy, int index, int spin)
{
    uint32_t val;

    val = mii_read_register(sc, phy, index);
    if (val == 0) {
	do {
	    val = mii_read_register(sc, phy, index);
	    } while (spin && val == 0);
	}
    return val;
}

static void
mii_write_register(t3_ether_t *sc, int phy, int index, uint16_t value)
{
    uint32_t mode;
    uint32_t comm, val;
    int   i;

    mode = READCSR(sc, R_MI_MODE);

    comm = (V_MICOMM_CMD_WR | V_MICOMM_PHY(phy) | V_MICOMM_REG(index)
	    | V_MICOMM_DATA(value) | M_MICOMM_BUSY);
    WRITECSR(sc, R_MI_COMM, comm);

    for (i = 0; i < MII_MAX_RETRIES; i++) {
	val = READCSR(sc, R_MI_COMM);
	if ((val & M_MICOMM_BUSY) == 0)
	    break;
	}	
    if (i == MII_MAX_RETRIES)
	xprintf("%s: mii_write_register: MII always busy\n", t3_devname(sc));

}

static int
mii_probe(t3_ether_t *sc)
{
#if T3_AUTOPOLL           /* With autopolling, the code below is not reliable.  */
    return 1;     /* Guaranteed for integrated PHYs */
#else
    int i;
    uint16_t id1, id2;

    for (i = 0; i < 32; i++) {
        id1 = mii_read_register(sc, i, MII_PHYIDR1);
	id2 = mii_read_register(sc, i, MII_PHYIDR2);
	if ((id1 != 0x0000 && id1 != 0xFFFF) ||
	    (id2 != 0x0000 && id2 != 0xFFFF)) {
	    if (id1 != id2) return i;
	    }
	}
    return -1;
#endif
}

static uint16_t
mii_read_shadow_register(t3_ether_t *sc, int index, int shadow_addr)
{
        uint16_t val;

#if T3_DEBUG
	xprintf("\nmii_read_shadow_register: reg=0x%X shadow=0x%X\n", index, shadow_addr);
#endif

	/* write to the shadow register first with the correct shadow address and write disabled */
	mii_write_register(sc, sc->phy_addr, index, (shadow_addr & ~SHDW_WR_EN) );
	
	/* read from the shadow register */
	val = mii_read_register(sc, sc->phy_addr, index);

#if T3_DEBUG
	xprintf("mii_read_shadow_register: reg=0x%X shadow=0x%X value=0x%X\n", index, shadow_addr, val);
#endif

	return(val);
}

static void
mii_write_shadow_register(t3_ether_t *sc, int index, int shadow_val)
{
        uint16_t val;

#if T3_DEBUG
	xprintf("\nmii_write_shadow_register: reg=0x%X shadow=0x%X\n", index, (shadow_val | SHDW_WR_EN) );
#endif

	/* write to the shadow register first with the correct shadow address and write enabled */
	mii_write_register(sc, sc->phy_addr, index, (shadow_val | SHDW_WR_EN));
	
	/* read from the shadow register */
	val = mii_read_shadow_register(sc, index, shadow_val);
	
#if T3_DEBUG
	xprintf("mii_write_shadow_register: reg=0x%X shadow=0x%X val=0x%X\n", index, shadow_val, val);
#endif
}

#if T3_DEBUG
#define OUI_BCM     0x001018
#define IDR_BCM     0x000818
/* 5400: 4, 5401: 5, 5411: 6, 5421: e, 5701: 11 */

static void
mii_dump(t3_ether_t *sc, const char *label)
{
    int i;
    uint16_t  r;
    uint32_t  idr, part;

    xprintf("%s, MII:\n", label);
    idr = part = 0;

    /* Required registers */
    for (i = 0x0; i <= 0x6; ++i) {
	r = mii_read_register(sc, sc->phy_addr, i);
	xprintf(" REG%02X: %04X", i, r);
	if (i == 3 || i == 6)
	    xprintf("\n");
	if (i == MII_PHYIDR1) {
	    idr |= r << 6;
	    }
	else if (i == MII_PHYIDR2) {
	    idr |= (r >> 10) & 0x3F;
	    part = (r >> 4) & 0x3F;
	    }
	}

    /* GMII extensions */
    for (i = 0x9; i <= 0xA; ++i) {
	r = mii_read_register(sc, sc->phy_addr, i);
	xprintf(" REG%02X: %04X", i, r);
	}
    r = mii_read_register(sc, sc->phy_addr, 0xF);
    xprintf(" REG%02X: %04X\n", 0xF, r);

    /* Broadcom extensions (54xx family) */
    if (idr == IDR_BCM) {
	for (i = 0x10; i <= 0x14; i++) {
	    r = mii_read_register(sc, sc->phy_addr, i);
	    xprintf(" REG%02X: %04X", i, r);
	    }
	xprintf("\n");
	for (i = 0x18; i <= 0x1A; i++) {
	    r = mii_read_register(sc, sc->phy_addr, i);
	    xprintf(" REG%02X: %04X", i, r);
	    }
	xprintf("\n");
	}
}
#else
#define mii_dump(sc,label)
#endif

static void
mii_enable_interrupts(t3_ether_t *sc)
{
  mii_write_register(sc, sc->phy_addr, MII_IMR, ~M_INT_LINKCHNG);
}


/* For 5700/5701, LINKCHNG is read-only in the status register and
   cleared by writing to CFGCHNG | SYNCCHNG.  For the 5705
   (empirically), LINKCHNG is cleared by writing a one, while CFGCHNG
   and SYNCCHNG are unimplemented.  Thus we can safely clear the
   interrupt by writing ones to all the above bits.  */

#define M_LINKCHNG_CLR \
    (M_EVT_LINKCHNG | M_MACSTAT_CFGCHNG | M_MACSTAT_SYNCCHNG)

static int
mii_poll(t3_ether_t *sc)
{
    uint32_t  macstat;
    uint16_t  status, ability, xability;
    uint16_t isr;

    macstat = READCSR(sc, R_MAC_STATUS);
    if ((macstat & (M_EVT_LINKCHNG | M_EVT_MIINT)) != 0)
	WRITECSR(sc, R_MAC_STATUS, M_LINKCHNG_CLR);

    /* BMSR has read-to-clear bits; read twice.  */
    
    status = mii_read_register(sc, sc->phy_addr, MII_BMSR);
    status = mii_read_register_v(sc, sc->phy_addr, MII_BMSR, 1);
    ability = mii_read_register_v(sc, sc->phy_addr, MII_ANLPAR, 0);
    if (status & BMSR_1000BT_XSR)
	xability = mii_read_register_v(sc, sc->phy_addr, MII_K1STSR, 0);
    else
	xability = 0;
    isr = mii_read_register(sc, sc->phy_addr, MII_ISR);

    if (status != sc->phy_status
	|| ability != sc->phy_ability || xability != sc->phy_xability) {
#if T3_DEBUG
	xprintf("[%04x]", isr);
	xprintf((macstat & (M_EVT_LINKCHNG | M_EVT_MIINT)) != 0 ? "+" : "-");
      
	if (status != sc->phy_status)
	    xprintf(" ST: %04x %04x", sc->phy_status, status);
	if (ability != sc->phy_ability)
	    xprintf(" AB: %04x %04x", sc->phy_ability, ability);
	if (xability != sc->phy_xability)
	    xprintf(" XA: %04x %04x", sc->phy_xability, xability);
	xprintf("\n");
#endif
        sc->phy_status = status;
	sc->phy_ability = ability;
	sc->phy_xability = xability;
	return 1;
	}
    else if ((macstat & (M_EVT_LINKCHNG | M_EVT_MIINT)) != 0) {
	isr = mii_read_register(sc, sc->phy_addr, MII_ISR);
	}
    return 0;
}

static void
mii_set_speed(t3_ether_t *sc, int speed)
{
    uint16_t  control;

    control = mii_read_register(sc, sc->phy_addr, MII_BMCR);

    control &= ~(BMCR_ANENABLE | BMCR_RESTARTAN);
    mii_write_register(sc, sc->phy_addr, MII_BMCR, control);
    control &= ~(BMCR_SPEED0 | BMCR_SPEED1 | BMCR_DUPLEX);

    switch (speed) {
	case ETHER_SPEED_10HDX:
	default:
	    break;
	case ETHER_SPEED_10FDX:
	    control |= BMCR_DUPLEX;
	    break;
	case ETHER_SPEED_100HDX:
	    control |= BMCR_SPEED100;
	    break;
	case ETHER_SPEED_100FDX:
	    control |= BMCR_SPEED100 | BMCR_DUPLEX ;
	    break;
	}

    mii_write_register(sc, sc->phy_addr, MII_BMCR, control);
}

static void
mii_autonegotiate(t3_ether_t *sc)
{
    uint16_t  control, status, remote, xremote;
    unsigned int  timeout;
    int linkspeed;
    uint32_t mode, ledCtrl; 
 
    linkspeed = ETHER_SPEED_UNKNOWN;

    /* Read twice to clear latching bits */
    status = mii_read_register(sc, sc->phy_addr, MII_BMSR);
    status = mii_read_register_v(sc, sc->phy_addr, MII_BMSR, 1);
    mii_dump(sc, "query PHY");

    if ((status & (BMSR_AUTONEG | BMSR_LINKSTAT)) ==
        (BMSR_AUTONEG | BMSR_LINKSTAT))
	control = mii_read_register(sc, sc->phy_addr, MII_BMCR);
    else {
	for (timeout = 4*CFE_HZ; timeout > 0; timeout -= CFE_HZ/2) {
	    status = mii_read_register(sc, sc->phy_addr, MII_BMSR);
	    if ((status & BMSR_ANCOMPLETE) != 0)
		break;
	    cfe_sleep(CFE_HZ/2);
	    }
	}

    remote = mii_read_register_v(sc, sc->phy_addr, MII_ANLPAR, 0);
    
    mode = READCSR(sc, R_MAC_MODE);

    xprintf("%s: Link speed: ", t3_devname(sc));
    if ((status & BMSR_ANCOMPLETE) != 0) {
	/* A link partner was negogiated... */

	if (status & BMSR_1000BT_XSR)
	    xremote = mii_read_register_v(sc, sc->phy_addr, MII_K1STSR, 0);
	else
	    xremote = 0;

	mode &= ~(M_MACM_PORTMODE | M_MACM_HALFDUPLEX);

	if ((xremote & K1STSR_LP1KFD) != 0) {
	    xprintf("1000BaseT FDX\n");
	    linkspeed = ETHER_SPEED_1000FDX;
	    mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_GMII);
	    }
	else if ((xremote & K1STSR_LP1KHD) != 0) {
	    xprintf("1000BaseT HDX\n");
	    linkspeed = ETHER_SPEED_1000HDX;
	    mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_GMII) | M_MACM_HALFDUPLEX;
	    }
	else if ((remote & ANLPAR_TXFD) != 0) {
	    xprintf("100BaseT FDX\n");
	    linkspeed = ETHER_SPEED_100FDX;	 
	    mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII);
	    }
	else if ((remote & ANLPAR_TXHD) != 0) {
	    xprintf("100BaseT HDX\n");
	    linkspeed = ETHER_SPEED_100HDX;	 
	    mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII) | M_MACM_HALFDUPLEX;
	    }
	else if ((remote & ANLPAR_10FD) != 0) {
	    xprintf("10BaseT FDX\n");
	    linkspeed = ETHER_SPEED_10FDX;	 
	    mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII);
	    }
	else if ((remote & ANLPAR_10HD) != 0) {
	    xprintf("10BaseT HDX\n");
	    linkspeed = ETHER_SPEED_10HDX;	 
	    mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII) | M_MACM_HALFDUPLEX;
	    }

	/* In order for the 5750 core in BCM4785 chip to work properly
 	 * in RGMII mode, the Led Control Register must be set up.
 	 */
	if ((sc->flags & (T3_SB_CORE | T3_RGMII_MODE)) == (T3_SB_CORE | T3_RGMII_MODE)) {
		ledCtrl = READCSR(sc, R_MAC_LED_CTRL);
		ledCtrl &= ~(M_LEDCTRL_1000MBPS | M_LEDCTRL_100MBPS);

		if((linkspeed == ETHER_SPEED_10FDX) || (linkspeed == ETHER_SPEED_10HDX))
			ledCtrl |= M_LEDCTRL_OVERRIDE;
		else if ((linkspeed == ETHER_SPEED_100FDX) || (linkspeed == ETHER_SPEED_100HDX))
			ledCtrl |= (M_LEDCTRL_OVERRIDE | M_LEDCTRL_100MBPS);
		else /* 1000MBPS */
			ledCtrl |= (M_LEDCTRL_OVERRIDE | M_LEDCTRL_1000MBPS);

		WRITECSR(sc, R_MAC_LED_CTRL, ledCtrl);

		cfe_usleep(40);;
	}

	WRITECSR(sc, R_MAC_MODE, mode);
    }
    else {
	/* no link partner convergence */
	xprintf("Unknown\n");
	linkspeed = ETHER_SPEED_UNKNOWN;
	remote = xremote = 0;

	/* If 5750 core in RGMII mode, set the speed to 1000 Mbps */
	if ((sc->flags & (T3_SB_CORE | T3_RGMII_MODE)) == (T3_SB_CORE | T3_RGMII_MODE)) {
		ledCtrl = READCSR(sc, R_MAC_LED_CTRL);
		ledCtrl &= ~(M_LEDCTRL_1000MBPS | M_LEDCTRL_100MBPS);
		ledCtrl |= (M_LEDCTRL_OVERRIDE | M_LEDCTRL_1000MBPS);
		WRITECSR(sc, R_MAC_LED_CTRL, ledCtrl);
		cfe_usleep(40);
	}
	mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_GMII);
	WRITECSR(sc, R_MAC_MODE, mode);
    }

    sc->linkspeed = linkspeed;

    status = mii_read_register_v(sc, sc->phy_addr, MII_BMSR, 1);
    (void)mii_read_register(sc, sc->phy_addr, MII_ISR);

    sc->phy_status = status;
    sc->phy_ability = remote;
    sc->phy_xability = xremote;

    mii_dump(sc, "final PHY");
}

static void
t3_force_speed(t3_ether_t *sc, int linkspeed)
{
    uint32_t mode, ledCtrl;

    
    mode = READCSR(sc, R_MAC_MODE);
	mode &= ~(M_MACM_PORTMODE | M_MACM_HALFDUPLEX);

    xprintf("%s: Link speed: ", t3_devname(sc));

	switch (linkspeed)
	{
		case ETHER_SPEED_1000FDX:
			xprintf("1000BaseT FDX\n");
			mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_GMII);
			break;
		case ETHER_SPEED_1000HDX:
			xprintf("1000BaseT HDX\n");
			mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_GMII) | M_MACM_HALFDUPLEX;
			break;
		case ETHER_SPEED_100FDX:
			xprintf("100BaseT FDX\n");
			mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII);
			break;
		case ETHER_SPEED_100HDX:
			xprintf("100BaseT HDX\n");
			mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII) | M_MACM_HALFDUPLEX;
			break;
		case ETHER_SPEED_10FDX:
			xprintf("10BaseT FDX\n");
			mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII);
			break;
		case ETHER_SPEED_10HDX:
			xprintf("10BaseT HDX\n");
			mode |= V_MACM_PORTMODE(K_MACM_PORTMODE_MII) | M_MACM_HALFDUPLEX;
			break;
		default:
			xprintf("Unknown\n");
			break;
	}
	
	/* In order for the 5750 core in BCM4785 chip to work properly
	 * in RGMII mode, the Led Control Register must be set up.
	 */
	if ((sc->flags & (T3_SB_CORE | T3_RGMII_MODE)) == (T3_SB_CORE | T3_RGMII_MODE)) {
		ledCtrl = READCSR(sc, R_MAC_LED_CTRL);
		ledCtrl &= ~(M_LEDCTRL_1000MBPS | M_LEDCTRL_100MBPS);

		if((linkspeed == ETHER_SPEED_10FDX) || (linkspeed == ETHER_SPEED_10HDX))
			ledCtrl |= M_LEDCTRL_OVERRIDE;
		else if ((linkspeed == ETHER_SPEED_100FDX) || (linkspeed == ETHER_SPEED_100HDX))
			ledCtrl |= (M_LEDCTRL_OVERRIDE | M_LEDCTRL_100MBPS);
		else /* 1000MBPS */
			ledCtrl |= (M_LEDCTRL_OVERRIDE | M_LEDCTRL_1000MBPS);

		WRITECSR(sc, R_MAC_LED_CTRL, ledCtrl);

		cfe_usleep(40);
	}
			
	WRITECSR(sc, R_MAC_MODE, mode);

    sc->linkspeed = linkspeed;
    sc->phy_status = 0;
    sc->phy_ability = 0;
    sc->phy_xability = 0;
}

static void
t3_clear(t3_ether_t *sc, unsigned reg, uint32_t mask)
{
    uint32_t val;
    int timeout;

    val = READCSR(sc, reg);
    val &= ~mask;
    WRITECSR(sc, reg, val);
    val = READCSR(sc, reg);

    for (timeout = 4000; (val & mask) != 0 && timeout > 0; timeout -= 100) {
	cfe_usleep(100);
	val = READCSR(sc, reg);
	}
    if (timeout <= 0)
	xprintf("%s: cannot clear %04X/%08X\n", t3_devname(sc), reg, (unsigned int)mask);
}


/* The following functions collectively implement the recommended
   BCM5700 Initialization Procedure (Section 8: Device Control) */

static int
t3_coldreset(t3_ether_t *sc)
{
    pcireg_t cmd;
    pcireg_t bhlc, subsysid;
    pcireg_t bar0, bar1;
    pcireg_t cmdx;
    uint32_t mhc, mcr, mcfg;
    uint32_t mode;
    int timeout;
    uint32_t magic;

    /* Steps 1-18 */
    /* Enable memory, also clear R/WC status bits (1) */
    cmd = pci_conf_read(sc->tag, PCI_COMMAND_STATUS_REG);
    cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
    cmd &= ~PCI_COMMAND_PARITY_ENABLE;
    cmd &= ~PCI_COMMAND_SERR_ENABLE;
    pci_conf_write(sc->tag, PCI_COMMAND_STATUS_REG, cmd);
    
    /* Clear and disable INTA output. (2) */
    mhc = READCSR(sc, R_MISC_HOST_CTRL);
    mhc |= M_MHC_MASKPCIINT | M_MHC_CLEARINTA;
    WRITECSR(sc, R_MISC_HOST_CTRL, mhc);

    /* Save some config registers modified by core clock reset (3). */
    bhlc = pci_conf_read(sc->tag, PCI_BHLC_REG);
    subsysid = pci_conf_read(sc->tag, PCI_SUBSYS_ID_REG);
    /* Empirically, these are clobbered too. */
    bar0 = pci_conf_read(sc->tag, PCI_MAPREG(0));
    bar1 = pci_conf_read(sc->tag, PCI_MAPREG(1));

    /* Reset the core clocks (4, 5). */
    mcfg = READCSR(sc, R_MISC_CFG);
    mcfg |= M_MCFG_CORERESET;
    WRITECSR(sc, R_MISC_CFG, mcfg);
    cfe_usleep(100);    /* 100 usec delay */

    /* NB: Until the BARs are restored and reenabled, only PCI
       configuration reads and writes will succeed.  */

    /* Reenable MAC memory (7) */
    pci_conf_write(sc->tag, PCI_MAPREG(0), bar0);
    pci_conf_write(sc->tag, PCI_MAPREG(1), bar1);
    (void)pci_conf_read(sc->tag, PCI_MAPREG(1));  /* push */
    pci_conf_write(sc->tag, PCI_COMMAND_STATUS_REG, cmd);
    (void)pci_conf_read(sc->tag, PCI_COMMAND_STATUS_REG);  /* push */

    /* Undo some of the resets (6) */
    mhc = READCSR(sc, R_MISC_HOST_CTRL);
    mhc |= M_MHC_MASKPCIINT;
    WRITECSR(sc, R_MISC_HOST_CTRL, mhc);

    /* Verify that core clock resets completed and autocleared. */
    mcfg = READCSR(sc, R_MISC_CFG);
    if ((mcfg & M_MCFG_CORERESET) != 0) {
	    xprintf("bcm5700: core clocks stuck in reset\n");
    }

    /* Configure PCI-X (8) */
    if (!(sc->device == K_PCI_ID_BCM5705 ||  sc->device == K_PCI_ID_BCM5750)) {
	    cmdx = pci_conf_read(sc->tag, PCI_PCIX_CMD_REG);
	    cmdx &= ~PCIX_CMD_RLXORDER_ENABLE;
	    pci_conf_write(sc->tag, PCI_PCIX_CMD_REG, cmdx);
    }

    if (sc->flags & T3_SB_CORE) {
#define HALT_CPU 0x400
		int ndx;

		/* Bang on halt request until it sticks */
		for (ndx = 0; ndx < 10000; ++ndx) {
			WRITECSR(sc, R_RX_RISC_STATE, 0xffffffff);
			WRITECSR(sc, R_RX_RISC_MODE, HALT_CPU);

			if ((READCSR(sc, R_RX_RISC_MODE) & HALT_CPU) == HALT_CPU)
				break;
		}

		/* One more time */
		WRITECSR(sc, R_RX_RISC_STATE, 0xffffffff);
		WRITECSR(sc, R_RX_RISC_MODE, HALT_CPU);
		(void)READCSR(sc, R_RX_RISC_MODE);

		cfe_usleep(10);

#undef HALT_CPU
    }

    /* Enable memory arbiter (9)  */
    mode = READCSR(sc, R_MEM_MODE);
    mode |= M_MAM_ENABLE;    /* enable memory arbiter */
    WRITECSR(sc, R_MEM_MODE, mode);

    /* Assume no external SRAM for now (10) */

    /* Set up MHC for endianness and write enables (11-15) */
    mhc = READCSR(sc, R_MISC_HOST_CTRL);
    /* Since we use match-bits for Direct PCI access, don't swap bytes. */
    mhc &= ~M_MHC_ENBYTESWAP;
#ifdef __MIPSEL
    mhc |= M_MHC_ENWORDSWAP;
#endif
#ifdef __MIPSEB
#if PIOSWAP
    mhc |= M_MHC_ENWORDSWAP;
#endif
#endif
    mhc |= M_MHC_ENINDIRECT | M_MHC_ENPCISTATERW | M_MHC_ENCLKCTRLRW;
    WRITECSR(sc, R_MISC_HOST_CTRL, mhc);

    /* Set byte swapping (16, 17) */
    mcr = READCSR(sc, R_MODE_CTRL);
#ifdef __MIPSEL
    mcr |= M_MCTL_BSWAPDATA | M_MCTL_WSWAPDATA;
    mcr |= M_MCTL_WSWAPCTRL;
#endif
#ifdef __MIPSEB
#if MATCH_BYTES
    mcr |= M_MCTL_BSWAPDATA | M_MCTL_WSWAPDATA;
    mcr |= M_MCTL_BSWAPCTRL | M_MCTL_WSWAPCTRL;
#else
    mcr &= ~(M_MCTL_BSWAPCTRL | M_MCTL_BSWAPDATA);
    mcr |= M_MCTL_WSWAPCTRL | M_MCTL_WSWAPDATA;
#endif
#endif
    WRITECSR(sc, R_MODE_CTRL, mcr);

    /* no firmware in BCM4785 */
    if (!(sc->flags & T3_SB_CORE)) {	
	    /* Disable PXE restart, wait for firmware (18, 19) */
	    for (timeout = 2 * CFE_HZ; timeout > 0; timeout--) {
		    WRITECSR(sc, R_MEMWIN_BASE_ADDR, A_PXE_MAILBOX);
		    magic = READCSR(sc, R_MEMWIN_DATA);

		    if (magic == ~T3_MAGIC_NUMBER)
			    break;
	    
		    cfe_sleep(1);
	    }

	    if (timeout == 0)
		    xprintf("bcm5700: no firmware rendevous\n");
    }

    WRITECSR(sc, R_MEMWIN_BASE_ADDR, 0);	/* restore default memory window */


    /* Clear Ethernet MAC Mode (20) */
    WRITECSR(sc, R_MAC_MODE, 0x00000000);
    (void)READCSR(sc, R_MAC_MODE);
    cfe_usleep(40);

    /* Restore remaining config registers (21) */
    pci_conf_write(sc->tag, PCI_BHLC_REG, bhlc);
    pci_conf_write(sc->tag, PCI_SUBSYS_ID_REG, subsysid);

	return(0);
}

static int
t3_warmreset(t3_ether_t *sc)
{
    uint32_t mode;

    /* Enable memory arbiter (9)  */
    mode = READCSR(sc, R_MEM_MODE);
    mode |= M_MAM_ENABLE;    /* enable memory arbiter */
    WRITECSR(sc, R_MEM_MODE, mode);

    /* Clear Ethernet MAC Mode (20) */
    WRITECSR(sc, R_MAC_MODE, 0x00000000);

    return 0;
}


static int
t3_init_registers(t3_ether_t *sc)
{
    unsigned offset;
    uint32_t dmac, mcr, mcfg;

    /* Steps 22-29 */

    /* Clear MAC statistics block (22) */
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750)) {
        for (offset = A_MAC_STATS; offset < A_MAC_STATS+L_MAC_STATS; offset += 4) {
	        WRITEMEM(sc, offset, 0);
	    }
	}

    /* Clear driver status memory region (23) */
    /* ASSERT (sizeof(t3_status_t) == L_MAC_STATUS) */
    memset((uint8_t *)sc->status, 0, sizeof(t3_status_t));

    /* Set up PCI DMA control (24) */
    dmac = READCSR(sc, R_DMA_RW_CTRL);
    dmac &= ~(M_DMAC_RDCMD | M_DMAC_WRCMD | M_DMAC_MINDMA);
    dmac |= V_DMAC_RDCMD(K_PCI_MEMRD) | V_DMAC_WRCMD(K_PCI_MEMWR);
    switch (sc->device) {
	case K_PCI_ID_BCM5700:
	case K_PCI_ID_BCM5701:
	case K_PCI_ID_BCM5702:
	    dmac |= V_DMAC_MINDMA(0xF);    /* "Recommended" */
	    break;
	default:
	    dmac |= V_DMAC_MINDMA(0x0);
	    break;
	}
    if (sc->flags & T3_SB_CORE) {
	if ((sc->sih->chip == BCM4785_CHIP_ID) && (sc->sih->chiprev < 2))
	    dmac |= V_DMAC_ONEDMA(1);
    }
    WRITECSR(sc, R_DMA_RW_CTRL, dmac);

    mcr = READCSR(sc, R_MODE_CTRL);
#ifdef __MIPSEL
    mcr |= M_MCTL_BSWAPDATA | M_MCTL_WSWAPDATA;
    mcr |= M_MCTL_WSWAPCTRL;
#endif
#ifdef __MIPSEB
#if MATCH_BYTES
    mcr |= M_MCTL_BSWAPDATA | M_MCTL_WSWAPDATA;
    mcr |= M_MCTL_BSWAPCTRL | M_MCTL_WSWAPCTRL;
#else
    mcr &= ~(M_MCTL_BSWAPCTRL | M_MCTL_BSWAPDATA);
    mcr |= M_MCTL_WSWAPCTRL | M_MCTL_WSWAPDATA;
#endif
#endif
    WRITECSR(sc, R_MODE_CTRL, mcr);

    /* Configure host rings (26) */
    mcr |= M_MCTL_HOSTBDS;
    WRITECSR(sc, R_MODE_CTRL, mcr);

    /* Indicate driver ready, disable checksums (27, 28) */
    mcr |= M_MCTL_HOSTUP;
    mcr |= (M_MCTL_NOTXPHSUM | M_MCTL_NORXPHSUM);
    WRITECSR(sc, R_MODE_CTRL, mcr);

    /* Configure timer (29) */
    mcfg = READCSR(sc, R_MISC_CFG);
    mcfg &= ~M_MCFG_PRESCALER;
    mcfg |= V_MCFG_PRESCALER(66-1);    /* 66 MHz */
    WRITECSR(sc, R_MISC_CFG, mcfg);

    return 0;
}

static int
t3_init_pools(t3_ether_t *sc)
{
    uint32_t mode;
    int timeout;

    /* Steps 30-36.  These use "recommended" settings (p 150) */

    /* Configure the MAC memory pool (30) */
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        WRITECSR(sc, R_BMGR_MBUF_BASE, A_BUFFER_POOL);
        WRITECSR(sc, R_BMGR_MBUF_LEN, L_BUFFER_POOL);
    }
    else 
    {
        /* Note: manual appears to recommend not even writing these (?) */
        /* WRITECSR(sc, R_BMGR_MBUF_BASE, A_RXMBUF); */
        /* WRITECSR(sc, R_BMGR_MBUF_LEN, 0x8000); */
    }

    /* Configure the MAC DMA resource pool (31) */
    WRITECSR(sc, R_BMGR_DMA_BASE, A_DMA_DESCS);
    WRITECSR(sc, R_BMGR_DMA_LEN,  L_DMA_DESCS);

    /* Configure the MAC memory watermarks (32) */
    if(sc->device == K_PCI_ID_BCM5705 ||
       sc->device == K_PCI_ID_BCM5750)
    {
		WRITECSR(sc, R_BMGR_MBUF_DMA_LOW, 0x0);
		WRITECSR(sc, R_BMGR_MBUF_RX_LOW,  0x10);
		WRITECSR(sc, R_BMGR_MBUF_HIGH,    0x60);
    }
    else
    {
		WRITECSR(sc, R_BMGR_MBUF_DMA_LOW, 0x50);
		WRITECSR(sc, R_BMGR_MBUF_RX_LOW,  0x20);
		WRITECSR(sc, R_BMGR_MBUF_HIGH,    0x60);
    }

    /* Configure the DMA resource watermarks (33) */
    WRITECSR(sc, R_BMGR_DMA_LOW,   5);
    WRITECSR(sc, R_BMGR_DMA_HIGH, 10);
    
    /* Enable the buffer manager (34, 35) */
    mode = READCSR(sc, R_BMGR_MODE);
    mode |= (M_BMODE_ENABLE | M_BMODE_MBUFLOWATTN);
    WRITECSR(sc, R_BMGR_MODE, mode);
    for (timeout = CFE_HZ/2; timeout > 0; timeout -= CFE_HZ/10) {
	mode = READCSR(sc, R_BMGR_MODE);
	if ((mode & M_BMODE_ENABLE) != 0)
	    break;
	cfe_sleep(CFE_HZ/10);
	}
    if ((mode & M_BMODE_ENABLE) == 0)
	xprintf("bcm5700: buffer manager not enabled\n");

    /* Enable internal queues (36) */
    WRITECSR(sc, R_FTQ_RESET, 0xFFFFFFFF);
    cfe_sleep(1);
    WRITECSR(sc, R_FTQ_RESET, 0x00000000);

	return(0);
}

static int
t3_init_rings(t3_ether_t *sc)
{
    unsigned rcbp;
    int i;

    /* Steps 37-46 */

    /* Initialize RCBs for Standard Receive Buffer Ring (37) */
    WRITECSR(sc, R_STD_RCV_BD_RCB+RCB_HOST_ADDR_HIGH, 0);
    WRITECSR(sc, R_STD_RCV_BD_RCB+RCB_HOST_ADDR_LOW, PTR_TO_PCI(sc->rxp_std));
    WRITECSR(sc, R_STD_RCV_BD_RCB+RCB_NIC_ADDR, A_STD_RCV_RINGS);
    if(sc->device == K_PCI_ID_BCM5705 ||
       sc->device == K_PCI_ID_BCM5750)
    {
        WRITECSR(sc, R_STD_RCV_BD_RCB+RCB_CTRL, V_RCB_MAXLEN(512));
    }
    else
    {
        WRITECSR(sc, R_STD_RCV_BD_RCB+RCB_CTRL, V_RCB_MAXLEN(ETH_PKTBUF_LEN));
    }

    /* Disable RCBs for Jumbo and Mini Receive Buffer Rings (38,39) */
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
	{
	    WRITECSR(sc, R_JUMBO_RCV_BD_RCB+RCB_CTRL,
				RCB_FLAG_USE_EXT_RCV_BD | RCB_FLAG_RING_DISABLED);
		WRITECSR(sc, R_MINI_RCV_BD_RCB+RCB_CTRL, RCB_FLAG_RING_DISABLED);
	}

    /* Set BD ring replenish thresholds (40) */
    WRITECSR(sc, R_MINI_RCV_BD_THRESH, 128);
#if T3_BRINGUP
    WRITECSR(sc, R_STD_RCV_BD_THRESH, 1);
#else
    WRITECSR(sc, R_STD_RCV_BD_THRESH, 25);
#endif
    WRITECSR(sc, R_JUMBO_RCV_BD_THRESH, 16);

    /* Disable all send producer rings (41) */
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
	    for (rcbp = A_SND_RCB(1); rcbp <= A_SND_RCB(16); rcbp += RCB_SIZE)
		    WRITEMEM(sc, rcbp+RCB_CTRL, RCB_FLAG_RING_DISABLED);
    }

    /* Initialize send producer index registers (42) */
    for (i = 1; i <= TXP_MAX_RINGS; i++) {
	WRITEMBOX(sc, R_SND_BD_PI(i), 0);
	WRITEMBOX(sc, R_SND_BD_NIC_PI(i), 0);
	}

    /* Initialize send producer ring 1 (43) */
    WRITEMEM(sc, A_SND_RCB(1)+RCB_HOST_ADDR_HIGH, 0);
    WRITEMEM(sc, A_SND_RCB(1)+RCB_HOST_ADDR_LOW, PTR_TO_PCI(sc->txp_1));
    WRITEMEM(sc, A_SND_RCB(1)+RCB_CTRL, V_RCB_MAXLEN(TXP_RING_ENTRIES));
    if (!(sc->device == K_PCI_ID_BCM5705 ||
          sc->device == K_PCI_ID_BCM5750))
		WRITEMEM(sc, A_SND_RCB(1)+RCB_NIC_ADDR, A_SND_RINGS);

    /* Disable unused receive return rings (44) */
    for (rcbp = A_RTN_RCB(1); rcbp <= A_RTN_RCB(16); rcbp += RCB_SIZE)
	WRITEMEM(sc, rcbp+RCB_CTRL, RCB_FLAG_RING_DISABLED);

    /* Initialize receive return ring 1 (45) */
    WRITEMEM(sc, A_RTN_RCB(1)+RCB_HOST_ADDR_HIGH, 0);
    WRITEMEM(sc, A_RTN_RCB(1)+RCB_HOST_ADDR_LOW, PTR_TO_PCI(sc->rxr_1));
    WRITEMEM(sc, A_RTN_RCB(1)+RCB_CTRL, V_RCB_MAXLEN(sc->rxr_entries));
    WRITEMEM(sc, A_RTN_RCB(1)+RCB_NIC_ADDR, 0x0000);

    /* Initialize receive producer ring mailboxes (46) */
    WRITEMBOX(sc, R_RCV_BD_STD_PI, 0);
    WRITEMBOX(sc, R_RCV_BD_JUMBO_PI, 0);
    WRITEMBOX(sc, R_RCV_BD_MINI_PI, 0);

	return(0);
}

static int
t3_configure_mac(t3_ether_t *sc)
{
    uint32_t low, high;
    uint32_t seed;
    int i;

    /* Steps 47-52 */

    /* Configure the MAC unicast address (47) */
    high = (sc->hwaddr[0] << 8) | (sc->hwaddr[1]);
    low = ((sc->hwaddr[2] << 24) | (sc->hwaddr[3] << 16)
	   | (sc->hwaddr[4] << 8) | sc->hwaddr[5]);
    /* For now, use a single MAC address */
    WRITECSR(sc, R_MAC_ADDR1_HIGH, high);  WRITECSR(sc, R_MAC_ADDR1_LOW, low);
    WRITECSR(sc, R_MAC_ADDR2_HIGH, high);  WRITECSR(sc, R_MAC_ADDR2_LOW, low);
    WRITECSR(sc, R_MAC_ADDR3_HIGH, high);  WRITECSR(sc, R_MAC_ADDR3_LOW, low);
    WRITECSR(sc, R_MAC_ADDR4_HIGH, high);  WRITECSR(sc, R_MAC_ADDR4_LOW, low);

    /* Configure the random backoff seed (48) */
    seed = 0;
    for (i = 0; i < 6; i++)
      seed += sc->hwaddr[i];
    seed &= 0x3FF;
    WRITECSR(sc, R_TX_BACKOFF, seed);

    /* Configure the MTU (49) */
    WRITECSR(sc, R_RX_MTU, MAX_ETHER_PACK+VLAN_TAG_LEN);

    /* Configure the tx IPG (50) */
    WRITECSR(sc, R_TX_LENS,
	     V_TXLEN_SLOT(0x20) | V_TXLEN_IPG(0x6) | V_TXLEN_IPGCRS(0x2));

    /* Configure the default rx return ring 1 (51) */
    WRITECSR(sc, R_RX_RULES_CFG, V_RULESCFG_DEFAULT(1));

    /* Configure the receive lists and enable statistics (52) */
    WRITECSR(sc, R_RCV_LIST_CFG,
	     V_LISTCFG_GROUP(1) | V_LISTCFG_ACTIVE(1) | V_LISTCFG_BAD(1));
    /* was V_LISTCFG_DEFAULT(1) | V_LISTCFG_ACTIVE(16) | V_LISTCFG_BAD(1) */

    return 0;
}

static int
t3_enable_stats(t3_ether_t *sc)
{
    uint32_t ctrl;

    /* Steps 53-56 */

    /* Enable rx stats (53,54) */
    WRITECSR(sc, R_RCV_LIST_STATS_ENB, 0xFFFFFF);
    ctrl = READCSR(sc, R_RCV_LIST_STATS_CTRL);
    ctrl |= M_STATS_ENABLE;
    WRITECSR(sc, R_RCV_LIST_STATS_CTRL, ctrl);

    /* Enable tx stats (55,56) */
    WRITECSR(sc, R_SND_DATA_STATS_ENB, 0xFFFFFF);
    ctrl = READCSR(sc, R_SND_DATA_STATS_CTRL);
    ctrl |= (M_STATS_ENABLE | M_STATS_FASTUPDATE);
    WRITECSR(sc, R_SND_DATA_STATS_CTRL, ctrl);

    return 0;
}

static int
t3_init_coalescing(t3_ether_t *sc)
{
    uint32_t mode = 0;
    int timeout;

    /* Steps 57-68 */

    /* Disable the host coalescing engine (57, 58) */
    WRITECSR(sc, R_HOST_COAL_MODE, 0);    
    for (timeout = CFE_HZ/2; timeout > 0; timeout -= CFE_HZ/10) {
	mode = READCSR(sc, R_HOST_COAL_MODE);
	if (mode == 0)
	    break;
	cfe_sleep(CFE_HZ/10);
	}
    if (mode != 0)
	xprintf("bcm5700: coalescing engine not disabled\n");

    /* Set coalescing parameters (59-62) */
#if T3_BRINGUP
    WRITECSR(sc, R_RCV_COAL_TICKS, 0);
    WRITECSR(sc, R_RCV_COAL_MAX_CNT, 1);
#else
    WRITECSR(sc, R_RCV_COAL_TICKS, 150);
    WRITECSR(sc, R_RCV_COAL_MAX_CNT, 10);
#endif
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
	    WRITECSR(sc, R_RCV_COAL_INT_TICKS, 0);
    WRITECSR(sc, R_RCV_COAL_INT_CNT, 0);
#if T3_BRINGUP
    WRITECSR(sc, R_SND_COAL_TICKS, 0);
    WRITECSR(sc, R_SND_COAL_MAX_CNT, 1);
#else
    WRITECSR(sc, R_SND_COAL_TICKS, 150);
    WRITECSR(sc, R_SND_COAL_MAX_CNT, 10);
#endif
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    	WRITECSR(sc, R_SND_COAL_INT_TICKS, 0);
    WRITECSR(sc, R_SND_COAL_INT_CNT, 0);

    /* Initialize host status block address (63) */
    WRITECSR(sc, R_STATUS_HOST_ADDR, 0);
    WRITECSR(sc, R_STATUS_HOST_ADDR+4, PTR_TO_PCI(sc->status));

    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
	    /* Initialize host statistics block address (64) */
	    WRITECSR(sc, R_STATS_HOST_ADDR, 0);
	    WRITECSR(sc, R_STATS_HOST_ADDR+4, PTR_TO_PCI(sc->stats));

	    /* Set statistics block NIC address and tick count (65, 66) */
	    WRITECSR(sc, R_STATS_TICKS, 1000000);
	    WRITECSR(sc, R_STATS_BASE_ADDR, A_MAC_STATS);

	    /* Set status block NIC address (67) */
	    WRITECSR(sc, R_STATUS_BASE_ADDR, A_MAC_STATUS);
    }

    /* Select the status block transfer size. */
    if (sc->device == K_PCI_ID_BCM5700)
	    mode = 0;          /* Truncated transfers not supported */
    else
	    mode = V_HCM_SBSIZE(STATUS_BLOCK_SIZE(MAX_RI));
      
    /* Enable the host coalescing engine (68) */
    mode |= M_HCM_ENABLE;
    WRITECSR(sc, R_HOST_COAL_MODE, mode);    

    return(0);
}

static int
t3_init_dma(t3_ether_t *sc)
{
    uint32_t mode;

    /* Steps 69-87 */

    /* Enable receive BD completion, placement, and selector blocks (69-71) */
    WRITECSR(sc, R_RCV_BD_COMP_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);
    WRITECSR(sc, R_RCV_LIST_MODE, M_MODE_ENABLE);
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        WRITECSR(sc, R_RCV_LIST_SEL_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);
    }

    /* Enable DMA engines, enable and clear statistics (72, 73) */
    mode = READCSR(sc, R_MAC_MODE);
    mode |= (M_MACM_FHDEENB | M_MACM_RDEENB | M_MACM_TDEENB |
	     M_MACM_RXSTATSENB | M_MACM_RXSTATSCLR |
	     M_MACM_TXSTATSENB | M_MACM_TXSTATSCLR);
	
    WRITECSR(sc, R_MAC_MODE, (mode | M_MACM_RXSTATSCLR |M_MACM_TXSTATSCLR) );

    if(!(sc->flags & T3_NO_PHY))
    {
#if T3_AUTOPOLL
    WRITECSR(sc, R_MISC_LOCAL_CTRL, M_MLCTL_INTATTN);
#endif
    }
	
    /* Configure GPIOs (74) - skipped */

    /* Clear interrupt mailbox (75) */
    WRITEMBOX(sc, R_INT_MBOX(0), 0);

    /* Enable DMA completion block (76) */
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        WRITECSR(sc, R_DMA_COMP_MODE, M_MODE_ENABLE);
    }

    /* Configure write and read DMA modes (77, 78) */
    WRITECSR(sc, R_WR_DMA_MODE, M_MODE_ENABLE | M_ATTN_ALL);
    WRITECSR(sc, R_RD_DMA_MODE, M_MODE_ENABLE | M_ATTN_ALL);

	return(0);
}

static int
t3_init_enable(t3_ether_t *sc)
{
    uint32_t mhc;
    uint32_t pmcs;
#if T3_AUTOPOLL
    uint32_t mode, mask;
#endif
    int  i;

    /* Steps 79-97 */

    /* Enable completion functional blocks (79-82) */
    WRITECSR(sc, R_RCV_COMP_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        WRITECSR(sc, R_MBUF_FREE_MODE, M_MODE_ENABLE);
    }
    WRITECSR(sc, R_SND_DATA_COMP_MODE, M_MODE_ENABLE);
    WRITECSR(sc, R_SND_BD_COMP_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);

    /* Enable initiator functional blocks (83-86) */
    WRITECSR(sc, R_RCV_BD_INIT_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);
    WRITECSR(sc, R_RCV_DATA_INIT_MODE, M_MODE_ENABLE | M_RCVINITMODE_RTNSIZE);
    WRITECSR(sc, R_SND_DATA_MODE, M_MODE_ENABLE);
    WRITECSR(sc, R_SND_BD_INIT_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);

    /* Enable the send BD selector (87) */
    WRITECSR(sc, R_SND_BD_SEL_MODE, M_MODE_ENABLE | M_MODE_ATTNENABLE);

    /* Download firmware (88) - skipped */

    /* Enable the MAC (89,90) */
    WRITECSR(sc, R_TX_MODE, M_MODE_ENABLE);   /* optional flow control */
    WRITECSR(sc, R_RX_MODE, M_MODE_ENABLE);   /* other options */

    /* Disable auto-polling (91) */
    mii_access_init(sc);

    /* Configure power state (92) */
    pmcs = READCSR(sc, PCI_PMCSR_REG);
    pmcs &= ~PCI_PMCSR_STATE_MASK;
    pmcs |= PCI_PMCSR_STATE_D0;
    WRITECSR(sc, PCI_PMCSR_REG, pmcs);

    /* Some chips require a little time to power up */
    cfe_sleep(1);

    if(!(sc->flags & T3_NO_PHY))
    {
#if T3_AUTOPOLL
	    /* Program hardware LED control (93) */
	    WRITECSR(sc, R_MAC_LED_CTRL, 0x00);   /* LEDs at PHY layer */
#endif

#if T3_AUTOPOLL
	    /* Ack/clear link change events */
	    WRITECSR(sc, R_MAC_STATUS, M_LINKCHNG_CLR);
	    WRITECSR(sc, R_MI_STATUS, 0);

	    /* Enable autopolling */
	    mode = READCSR(sc, R_MI_MODE);
	    mode &= ~(0x1f << 16);
	    mode |= M_MIMODE_POLLING | (0x0c << 16);
	    WRITECSR(sc, R_MI_MODE, mode);

	    /* Enable link state attentions */
	    mask = READCSR(sc, R_MAC_EVENT_ENB);
	    mask |= M_EVT_LINKCHNG;
	    WRITECSR(sc, R_MAC_EVENT_ENB, mask);
#else
	    /* Initialize link (94) */
	    WRITECSR(sc, R_MI_STATUS, M_MISTAT_LINKED);

	    /* Start autonegotiation (95) - see t3_initlink below */

	    /* Setup multicast filters (96) */
	    for (i = 0; i < 4; i++)
		    WRITECSR(sc, R_MAC_HASH(i), 0);
#endif /* T3_AUTOPOLL */
    }
    else
    {
	    /* Initialize link (94) */
	    WRITECSR(sc, R_MI_STATUS, M_MISTAT_LINKED);

	    /* Start autonegotiation (95) - see t3_initlink below */

	    /* Setup multicast filters (96) */
	    for (i = 0; i < 4; i++)
		    WRITECSR(sc, R_MAC_HASH(i), 0);
    }

    /* Enable interrupts (97) */
    mhc = READCSR(sc, R_MISC_HOST_CTRL);
    mhc &= ~M_MHC_MASKPCIINT;
    WRITECSR(sc, R_MISC_HOST_CTRL, mhc);

    if ((sc->flags & T3_NO_PHY))
	    cfe_sleep(1);
	
    return(0);
}

static void
t3_initlink(t3_ether_t *sc)
{
    uint32_t mcr;

    if (!(sc->flags & T3_NO_PHY))
    {
	    sc->phy_addr = mii_probe(sc);
	    if (sc->phy_addr < 0) 
	    {
		xprintf("%s: no PHY found\n", t3_devname(sc));
		return;  
	    }
#if T3_DEBUG
	    xprintf("%s: PHY addr %d\n", t3_devname(sc), sc->phy_addr);
#endif

	    if (1)
		mii_autonegotiate(sc);
	    else
		mii_set_speed(sc, ETHER_SPEED_10HDX);

	    /* 
	    ** Change the 5461 PHY INTR//ENERGYDET LED pin to function as ENERGY DET  by
	    ** writing to the shadow control register 0x1c value 00100 | masks 
	    */
	    mii_write_shadow_register(sc, MII_SHADOW, (SHDW_SPR_CTRL | SHDW_NRG_DET) );

	    mii_enable_interrupts(sc);

	    mcr = READCSR(sc, R_MODE_CTRL);
	    mcr |= M_MCTL_MACINT;
	    WRITECSR(sc, R_MODE_CTRL, mcr);
    }
    else
    {
	    /* T3_NO_PHY means there is a ROBO switch, configure it */
	    robo_info_t *robo;

	    robo = bcm_robo_attach(sc->sih, sc, NULL,
	                           (miird_f)mii_read_register, (miiwr_f)mii_write_register);
	    if (robo == NULL) {
		    xprintf("robo_setup: failed to attach robo switch \n");
		    goto robo_fail;
	    }

        if (robo->devid == DEVID5325)
        {
            t3_force_speed(sc, ETHER_SPEED_100FDX);
        }
        else
        {
            t3_force_speed(sc, ETHER_SPEED_1000FDX);
        }

	    if (bcm_robo_enable_device(robo)) {
		    xprintf("robo_setup: failed to enable robo switch \n");
		    goto robo_fail;
	    }

	    /* Configure the switch to do VLAN */
	    if (bcm_robo_config_vlan(robo, sc->hwaddr)) {
		    xprintf("robo_setup: robo_config_vlan failed\n");
		    goto robo_fail;
	    }

	    /* Enable the switch */
	    if (bcm_robo_enable_switch(robo)) {
		    xprintf("robo_setup: robo_enable_switch failed\n");
robo_fail:
		    bcm_robo_detach(robo);
	    }
    }

    sc->mii_polling = 0;
    sc->phy_change = 0;
}

static void
t3_shutdownlink(t3_ether_t *sc)
{
    uint32_t mcr;

    mcr = READCSR(sc, R_MODE_CTRL);
    mcr &= ~M_MCTL_MACINT;
    WRITECSR(sc, R_MODE_CTRL, mcr);

    WRITECSR(sc, R_MAC_EVENT_ENB, 0);

    /* The manual is fuzzy about what to do with the PHY at this
       point.  Empirically, resetting the 5705 PHY (but not others)
       will cause it to get stuck in 10/100 MII mode.  */
    if (!(sc->flags & T3_NO_PHY))
    {
    	if (sc->device != K_PCI_ID_BCM5705)
		mii_write_register(sc, sc->phy_addr, MII_BMCR, BMCR_RESET);

	    sc->mii_polling = 0;
	    sc->phy_change = 0;
    }
}


static void
t3_hwinit(t3_ether_t *sc)
{
    if (sc->state != eth_state_on) {

	if (sc->state == eth_state_uninit) {
	    t3_coldreset(sc);
	    }
	else
	    t3_warmreset(sc);

	t3_init_registers(sc);
	t3_init_pools(sc);
	t3_init_rings(sc);
	t3_configure_mac(sc);
	t3_enable_stats(sc);
	t3_init_coalescing(sc);
	t3_init_dma(sc);
	t3_init_enable(sc);
#if T3_DEBUG
	dumpcsrs(sc, "end init");
#else
	(void)dumpcsrs;
#endif

	eeprom_access_init(sc);
#if T3_DEBUG
	{
	    uint32_t eeprom[0x100/4];
	    int i;
	    
	    cfe_sleep(1);
	    for (i = 0; i < 4; i++) {
		eeprom_read_range(sc, 0, 4, eeprom);
		}

	    eeprom_read_range(sc, 0, sizeof(eeprom), eeprom);
	    eeprom_dump_range("Boot Strap", eeprom, 0x00, 20);
	    eeprom_dump_range("Manufacturing Info", eeprom, 0x74, 140);
	}
#else
	(void)eeprom_read_range;
	(void)eeprom_dump_range;
#endif

	t3_initlink(sc);

	sc->state = eth_state_off;
	}
	
}

static void
t3_hwshutdown(t3_ether_t *sc)
{
    /* Receive path shutdown */
    t3_clear(sc, R_RX_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_RCV_BD_INIT_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_RCV_LIST_MODE, M_MODE_ENABLE);
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        t3_clear(sc, R_RCV_LIST_SEL_MODE, M_MODE_ENABLE);
    }
    t3_clear(sc, R_RCV_DATA_INIT_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_RCV_COMP_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_RCV_BD_COMP_MODE, M_MODE_ENABLE);

    /* Transmit path shutdown */
    t3_clear(sc, R_SND_BD_SEL_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_SND_BD_INIT_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_SND_DATA_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_RD_DMA_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_SND_DATA_COMP_MODE, M_MODE_ENABLE);
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        t3_clear(sc, R_DMA_COMP_MODE, M_MODE_ENABLE);
    }
    t3_clear(sc, R_SND_BD_COMP_MODE, M_MODE_ENABLE);
    t3_clear(sc, R_TX_MODE, M_MODE_ENABLE);

    /* Memory shutdown */
    t3_clear(sc, R_HOST_COAL_MODE, M_HCM_ENABLE);
    t3_clear(sc, R_WR_DMA_MODE, M_MODE_ENABLE);
    if(!(sc->device == K_PCI_ID_BCM5705 ||
         sc->device == K_PCI_ID_BCM5750))
    {
        t3_clear(sc, R_MBUF_FREE_MODE, M_MODE_ENABLE);
    }
    WRITECSR(sc, R_FTQ_RESET, 0xFFFFFFFF);
    cfe_sleep(1);
    WRITECSR(sc, R_FTQ_RESET, 0x00000000);
    t3_clear(sc, R_BMGR_MODE, M_BMODE_ENABLE);
    t3_clear(sc, R_MEM_MODE, M_MAM_ENABLE);

    t3_shutdownlink(sc);

    t3_coldreset(sc);

    sc->state = eth_state_uninit;
}


static void
t3_isr(void *arg)
{
    t3_ether_t *sc = (t3_ether_t *)arg;
    volatile t3_status_t *status = sc->status;
    uint32_t mac_status;
    int handled;

    do { 
	WRITEMBOX(sc, R_INT_MBOX(0), 1);

	handled = 0;
	mac_status = READCSR(sc, R_MAC_STATUS);  /* force ordering */
	status->status &= ~M_STATUS_UPDATED;
    
	if (status->index[RI(1)].return_p != sc->rxr_1_index) {
	    handled = 1;
	    if (IPOLL) sc->rx_interrupts++;  
	    t3_procrxring(sc);
	    }

	if (status->index[RI(1)].send_c != sc->txc_1_index) {
	    handled = 1;
	    if (IPOLL) sc->tx_interrupts++;  
	    t3_proctxring(sc);
	    }

	if ((status->status & M_STATUS_LINKCHNG) != 0) {
	    handled = 1;

	    if (!(sc->flags & T3_NO_PHY))
	    {
#if T3_AUTOPOLL
	    	WRITECSR(sc, R_MAC_STATUS, M_LINKCHNG_CLR);
#endif
	    }

	    WRITECSR(sc, R_MAC_STATUS, M_EVT_MICOMPLETE);

	    status->status &= ~M_STATUS_LINKCHNG;
	    sc->phy_change = 1;
	 }

	WRITEMBOX(sc, R_INT_MBOX(0), 0);
	(void)READMBOX(sc, R_INT_MBOX(0));  /* push */

#if !XPOLL
	if (!handled)
	    sc->bogus_interrupts++;
#endif

	} while ((status->status & M_STATUS_UPDATED) != 0);

    if (sc->rxp_std_index != sc->prev_rxp_std_index) {
	sc->prev_rxp_std_index = sc->rxp_std_index;
	WRITEMBOX(sc, R_RCV_BD_STD_PI, sc->rxp_std_index);
	}
}


static void
t3_clear_stats(t3_ether_t *sc)
{
    t3_stats_t zeros;

    if (sc->device == K_PCI_ID_BCM5705 ||
        sc->device == K_PCI_ID_BCM5750)
    	return;

    memset(&zeros, 0, sizeof(t3_stats_t));
    WRITEMBOX(sc, R_RELOAD_STATS_MBOX + 4, 0);
    WRITEMBOX(sc, R_RELOAD_STATS_MBOX, PTR_TO_PCI(&zeros));
}


static void
t3_start(t3_ether_t *sc)
{
    t3_hwinit(sc);

    sc->intmask = 0;

#if IPOLL
    cfe_request_irq(sc->irq, t3_isr, sc, CFE_IRQ_FLAGS_SHARED, 0);

    if (!(sc->flags & T3_NO_PHY))
    {
#if T3_AUTOPOLL
    	sc->intmask |= M_EVT_LINKCHNG;
#else
    	sc->intmask |= M_EVT_LINKCHNG | M_EVT_MIINT; 
#endif
    }
    else
    {
    	sc->intmask |= M_EVT_LINKCHNG | M_EVT_MIINT; 
    }
    WRITECSR(sc, R_MAC_EVENT_ENB, sc->intmask);
#endif

    /* Post some Rcv Producer buffers */
    sc->prev_rxp_std_index = sc->rxp_std_index;
    WRITEMBOX(sc, R_RCV_BD_STD_PI, sc->rxp_std_index);

    sc->state = eth_state_on;
}

static void
t3_stop(t3_ether_t *sc)
{
    WRITECSR(sc, R_MAC_EVENT_ENB, 0);
    sc->intmask = 0;
#if IPOLL
    cfe_free_irq(sc->irq, 0);
#endif

    if (sc->state == eth_state_on) {
	sc->state = eth_state_off;
	t3_hwshutdown(sc);
	t3_reinit(sc);
	}
}


static int t3_ether_open(cfe_devctx_t *ctx);
static int t3_ether_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int t3_ether_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int t3_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int t3_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int t3_ether_close(cfe_devctx_t *ctx);
static void t3_ether_poll(cfe_devctx_t *ctx, int64_t ticks);
static void t3_ether_reset(void *softc);

const static cfe_devdisp_t t3_ether_dispatch = {
    t3_ether_open,
    t3_ether_read,
    t3_ether_inpstat,
    t3_ether_write,
    t3_ether_ioctl,
    t3_ether_close,	
    t3_ether_poll,
    t3_ether_reset
};

cfe_driver_t bcm5700drv = {
    "BCM570x Ethernet",
    "eth",
    CFE_DEV_NETWORK,
    &t3_ether_dispatch,
    t3_ether_probe
};


static void
t3_delete_sc(t3_ether_t *sc)
{
    xprintf("BCM570x attach: No memory to complete probe\n");
    if (sc != NULL) {
	if (sc->txp_1 != NULL)
	    kfree_uncached(sc->txp_1);
	if (sc->rxr_1 != NULL)
	    kfree_uncached(sc->rxr_1);
	if (sc->rxp_std != NULL)
	    kfree_uncached(sc->rxp_std);
	if (sc->stats != NULL)
	    kfree_uncached((t3_stats_t *)sc->stats);
	if (sc->status != NULL)
	    kfree_uncached((t3_ether_t *)sc->status);
	KFREE(sc);
	}
}

static int
t3_ether_attach(cfe_driver_t *drv, pcitag_t tag, int index)
{
    t3_ether_t *sc;
    char descr[80];
    phys_addr_t pa;
    uint32_t base;
    uint32_t pcictrl;
    uint32_t addr;
    pcireg_t device, class;
    const char *devname;
    bool rgmii = FALSE;
    si_t *sih = NULL;
    int i;

    device = pci_conf_read(tag, PCI_ID_REG);
    class = pci_conf_read(tag, PCI_CLASS_REG);

    if (PCI_PRODUCT(device) == K_PCI_ID_BCM471F) {
	sih = si_kattach(SI_OSH);
	hndgige_init(sih, ++sigige, &rgmii);
    }

    pci_map_mem(tag, PCI_MAPREG(0), PCI_MATCH_BITS, &pa);
    base = (uint32_t)pa;

    sc = (t3_ether_t *) KMALLOC(sizeof(t3_ether_t), 0);
    if (sc == NULL) {
	t3_delete_sc(sc);
	return 0;
	}

    memset(sc, 0, sizeof(*sc));
    
    sc->status = NULL;
    sc->stats = NULL;

    sc->tag = tag;
    sc->device = PCI_PRODUCT(device);
    sc->revision = PCI_REVISION(class);
    /* (Some?) 5700s report the 5701 device code */
    sc->asic_revision = G_MHC_ASICREV(pci_conf_read(tag, R_MISC_HOST_CTRL));
    if (sc->device == K_PCI_ID_BCM5701
	&& (sc->asic_revision & 0xF000) == 0x7000)
	sc->device = K_PCI_ID_BCM5700;
    /* From now on we'll lose our identify to BCM5750 */
    if (sih) {
	    sc->flags |= rgmii ? T3_RGMII_MODE : 0;
	    sc->flags |= T3_SB_CORE;
	    if (getintvar(NULL, "boardflags") & BFL_ENETROBO)
		    sc->flags |= T3_NO_PHY;
	    sc->device = K_PCI_ID_BCM5750;
	    sc->sih = sih;
	    sc->siidx = sigige;
    }

    sc->status = (t3_status_t *) kmalloc_uncached(sizeof(t3_status_t), CACHE_ALIGN);
    if (sc->status == NULL) {
	t3_delete_sc(sc);
	return 0;
	}

    sc->stats = (t3_stats_t *) kmalloc_uncached(sizeof(t3_stats_t), CACHE_ALIGN);
    if (sc->stats == NULL) {
	t3_delete_sc(sc);
	return 0;
	}

    if (sc->device == K_PCI_ID_BCM5705 ||
        sc->device == K_PCI_ID_BCM5750)
	sc->rxr_entries = RXR_RING_ENTRIES_05;
    else
	sc->rxr_entries = RXR_RING_ENTRIES;

    sc->rxp_std =
        (t3_rcv_bd_t *) kmalloc_uncached(RXP_STD_ENTRIES*RCV_BD_SIZE, CACHE_ALIGN);
    sc->rxr_1 =
        (t3_rcv_bd_t *) kmalloc_uncached(sc->rxr_entries*RCV_BD_SIZE, CACHE_ALIGN);
    sc->txp_1 =
        (t3_snd_bd_t *) kmalloc_uncached(TXP_RING_ENTRIES*SND_BD_SIZE, CACHE_ALIGN);
    if (sc->rxp_std == NULL || sc->rxr_1 == NULL || sc->txp_1 == NULL) {
	t3_delete_sc(sc);
	return 0;
	}

    sc->regbase = base;

    /* NB: the relative base of memory depends on the access model */
    pcictrl = pci_conf_read(tag, R_PCI_STATE);
    sc->membase = base + 0x8000;       /* Normal mode: 32K window */
    sc->irq = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG) & 0xFF;

    sc->devctx = NULL;

    if (sc->flags & T3_SB_CORE) {
	    char etXmacaddr[] = "etXXXXmacaddr";
	    uint32_t low, high;

	    sprintf(etXmacaddr, "et%umacaddr", (unsigned int)sc->siidx);
	    bcm_ether_atoe(getvar(NULL, etXmacaddr),
	                   (struct ether_addr *)sc->hwaddr);
	    high = (sc->hwaddr[0] << 8) | (sc->hwaddr[1]);
	    low = ((sc->hwaddr[2] << 24) | (sc->hwaddr[3] << 16)
	           | (sc->hwaddr[4] << 8) | sc->hwaddr[5]);
	    /* For now, use a single MAC address */
	    WRITECSR(sc, R_MAC_ADDR1_HIGH, high);  WRITECSR(sc, R_MAC_ADDR1_LOW, low);
	    WRITECSR(sc, R_MAC_ADDR2_HIGH, high);  WRITECSR(sc, R_MAC_ADDR2_LOW, low);
	    WRITECSR(sc, R_MAC_ADDR3_HIGH, high);  WRITECSR(sc, R_MAC_ADDR3_LOW, low);
	    WRITECSR(sc, R_MAC_ADDR4_HIGH, high);  WRITECSR(sc, R_MAC_ADDR4_LOW, low);
    } else {
	    /* Assume on-chip firmware has initialized the MAC address. */
	    addr = READCSR(sc, R_MAC_ADDR1_HIGH);
	    for (i = 0; i < 2; i++)
		    sc->hwaddr[i] = (addr >> (8*(1-i))) & 0xff;
	    addr = READCSR(sc, R_MAC_ADDR1_LOW);
	    for (i = 0; i < 4; i++)
		    sc->hwaddr[2+i] = (addr >> (8*(3-i))) & 0xff;
    }

    t3_init(sc);

    sc->state = eth_state_uninit;

    /* print device info */
    switch (sc->device) {
    case K_PCI_ID_BCM5700:
	devname = "BCM5700"; break;
    case K_PCI_ID_BCM5701:
	devname = "BCM5701"; break;
    case K_PCI_ID_BCM5702:
	devname = "BCM5702"; break;
    case K_PCI_ID_BCM5703:
	devname = "BCM5703"; break;
    case K_PCI_ID_BCM5705:
	devname = "BCM5705"; break;
    case K_PCI_ID_BCM5750:
	devname = "BCM5750"; break;
    default:
	devname = "BCM570x"; break;
	}
    xsprintf(descr, "%s Ethernet at 0x%X", devname, (unsigned int)sc->regbase);
    printf("ge%d: %s\n", index, descr);

    cfe_attach(drv, sc, NULL, descr);
    return 1;
}

static void
t3_ether_probe(cfe_driver_t *drv,
	       unsigned long probe_a, unsigned long probe_b, 
	       void *probe_ptr)
{
    int index;
    int n;
		
    n = 0;
    index = 0;
    for (;;) {
	pcitag_t tag;
	pcireg_t device;

	if (pci_find_class(PCI_CLASS_NETWORK, index, &tag) != 0)
	   break;

	index++;

	device = pci_conf_read(tag, PCI_ID_REG);
	if (PCI_VENDOR(device) == K_PCI_VENDOR_BROADCOM) {
	    switch (PCI_PRODUCT(device)) {
		case K_PCI_ID_BCM5700:
		case K_PCI_ID_BCM5701:
		case K_PCI_ID_BCM5702:
		case K_PCI_ID_BCM5703:
		case K_PCI_ID_BCM5703a:
		case K_PCI_ID_BCM5703b:
		case K_PCI_ID_BCM5704C:
		case K_PCI_ID_BCM5705:
		case K_PCI_ID_BCM5750:
		case K_PCI_ID_BCM471F: 
		    t3_ether_attach(drv, tag, n);
		    n++;
		    break;
		default:
		    break;
		}
	    }
	}
}


/* The functions below are called via the dispatch vector for the Tigon 3 */

static int
t3_ether_open(cfe_devctx_t *ctx)
{
    t3_ether_t *sc = ctx->dev_softc;
    volatile t3_stats_t *stats = sc->stats;
    int i;

    if (sc->state == eth_state_on)
	t3_stop(sc);

    sc->devctx = ctx;

    for (i = 0; i < L_MAC_STATS/sizeof(uint64_t); i++)
	{
	stats->stats[i] = 0;
	}

    t3_start(sc);

    sc->rx_interrupts = sc->tx_interrupts = sc->bogus_interrupts = 0;
    t3_clear_stats(sc);

    if (XPOLL)
	    t3_isr(sc);

    return(0);
}

static int
t3_ether_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    t3_ether_t *sc = ctx->dev_softc;
    eth_pkt_t *pkt;
    int blen;

    if (XPOLL) t3_isr(sc);

    if (sc->state != eth_state_on) return -1;

    CS_ENTER(sc);
    pkt = (eth_pkt_t *) q_deqnext(&(sc->rxqueue));
    CS_EXIT(sc);

    if (pkt == NULL) {
	buffer->buf_retlen = 0;
	return 0;
	}

    blen = buffer->buf_length;
    if (blen > pkt->length) blen = pkt->length;

    memcpy(buffer->buf_ptr, pkt->buffer, blen);
    buffer->buf_retlen = blen;

    eth_free_pkt(sc, pkt);

    if (XPOLL) t3_isr(sc);
    return 0;
}

static int
t3_ether_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
    t3_ether_t *sc = ctx->dev_softc;

    if (XPOLL) t3_isr(sc);

    if (sc->state != eth_state_on) return -1;

    /* We avoid an interlock here because the result is a hint and an
       interrupt cannot turn a non-empty queue into an empty one. */
    inpstat->inp_status = (q_isempty(&(sc->rxqueue))) ? 0 : 1;

    return 0;
}

static int
t3_ether_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    t3_ether_t *sc = ctx->dev_softc;
    eth_pkt_t *pkt;
    int blen;

    if (XPOLL) t3_isr(sc);

    if (sc->state != eth_state_on) return -1;

    pkt = eth_alloc_pkt(sc);
    if (!pkt) return CFE_ERR_NOMEM;

    blen = buffer->buf_length;
    if (blen > pkt->length) blen = pkt->length;

    memcpy(pkt->buffer, buffer->buf_ptr, blen);
    pkt->length = blen;

    /*
	 * Ensure that the packet memory is flushed out of the data cache
	 * before posting it for transmission.
     */
    cfe_flushcache(CFE_CACHE_FLUSH_D);

    if (t3_transmit(sc, pkt) != 0) {
	eth_free_pkt(sc,pkt);
	return CFE_ERR_IOERR;
	}

    if (XPOLL) t3_isr(sc);
    return 0;
}

static int
t3_ether_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer) 
{
    t3_ether_t *sc = ctx->dev_softc;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_ETHER_GETHWADDR:
	    memcpy(buffer->buf_ptr, sc->hwaddr, sizeof(sc->hwaddr));
	    return 0;

	default:
	    return -1;
	}
}

static int
t3_ether_close(cfe_devctx_t *ctx)
{
    t3_ether_t *sc = ctx->dev_softc;
    volatile t3_stats_t *stats = sc->stats;
    uint32_t inpkts, outpkts, interrupts;
    int i;

    t3_stop(sc);

#if T3_BRINGUP
    for (i = 0; i < L_MAC_STATS/sizeof(uint64_t); i++) {
	if (stats->stats[i] != 0)
	    xprintf(" stats[%d] = %8lld\n", i, stats->stats[i]);
	}
#else
    (void) i;
#endif

    inpkts = stats->stats[ifHCInUcastPkts]
	      + stats->stats[ifHCInMulticastPkts]
	      + stats->stats[ifHCInBroadcastPkts];
    outpkts = stats->stats[ifHCOutUcastPkts]
	      + stats->stats[ifHCOutMulticastPkts]
	      + stats->stats[ifHCOutBroadcastPkts];
    interrupts = stats->stats[nicInterrupts];

    /* Empirically, counters on the 5705 are always zero.  */
    if (!(sc->device == K_PCI_ID_BCM5705 ||
          sc->device == K_PCI_ID_BCM5750)) {
	xprintf("%s: %d sent, %d received, %d interrupts\n",
	        t3_devname(sc), (int)outpkts, (int)inpkts, (int)interrupts);
	if (IPOLL) {
	    xprintf("  %d rx interrupts, %d tx interrupts",
	            (int)sc->rx_interrupts, (int)sc->tx_interrupts);
	    if (sc->bogus_interrupts != 0)
	        xprintf(", %d bogus interrupts", (int)sc->bogus_interrupts);
	    xprintf("\n");
	    }
	}

    sc->devctx = NULL;
    return 0;
}

static void
t3_ether_poll(cfe_devctx_t *ctx, int64_t ticks)
{
    t3_ether_t *sc = ctx->dev_softc;
    int changed;

    if(!(sc->flags & T3_NO_PHY)) {
    	if (sc->phy_change && sc->state != eth_state_uninit && !sc->mii_polling) {
		uint32_t mask;

		sc->mii_polling++;
		mask = READCSR(sc, R_MAC_EVENT_ENB);
		WRITECSR(sc, R_MAC_EVENT_ENB, 0);

		changed = mii_poll(sc);
		if (changed) {
			mii_autonegotiate(sc);
	    	}
		sc->phy_change = 0;
		sc->mii_polling--;

		WRITECSR(sc, R_MAC_EVENT_ENB, mask);
	}
    }
}

static void
t3_ether_reset(void *softc)
{
    t3_ether_t *sc = (t3_ether_t *)softc;

    /* Turn off the Ethernet interface. */

    if (sc->state == eth_state_on)
	t3_stop(sc);

    sc->state = eth_state_uninit;
}
