/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BC5821 crypto accelerator driver	File: dev_bcm5821.c
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

/*
   CFE Driver plus test programs for the BCM5820 and BCM5821 crypto
   coprocessor chips.
   Reference:
     BCM5821 Super-eCommerce Processor
     Data Sheet 5821-DS105-D1 (draft, 7/26/02)
     Broadcom Corp., 16215 Alton Parkway, Irvine, CA.
*/
   
/* The performance counter usage assumes a BCM11xx or BCM1250 part */

#include "sbmips.h"

#if !defined(_SB14XX_) && !defined(_RM5200_) && !defined(_RM7000_)
#define _SB1250_PERF_ 1
#else
#define _SB1250_PERF_ 0
#endif

#if _SB1250_PERF_
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#endif

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

#include "addrspace.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#include "cfe_devfuncs.h"
#include "cfe_irq.h"

#include "pcivar.h"
#include "pcireg.h"

#include "bcm5821.h"

/* The version that works by polling the CPU's Cause register doesn't
   do handshakes or checks to detect merged interrupts.  It currently
   works when the 5821 is on the direct PCI bus but can behave
   erratically when the 5821 is behind an LDT-to-PCI bridge that does
   interrupt mapping and relies on EOI. */

extern int32_t _getcause(void);		/* return value of CP0 CAUSE */

#define IMR_POINTER(cpu,reg) \
    ((volatile uint64_t *)(PHYS_TO_K1(A_IMR_REGISTER(cpu,reg))))

#define CACHE_LINE_SIZE  32

static void bcm5821_probe(cfe_driver_t *drv,
			  unsigned long probe_a, unsigned long probe_b, 
			  void *probe_ptr);

typedef struct bcm5821_state_s {
    uint32_t  regbase;
    uint8_t irq;
    pcitag_t tag;		/* tag for configuration registers */

    uint16_t  device;           /* chip device code */
    uint8_t revision;           /* chip revision */

} bcm5821_state_t;


/* Address mapping macros */

/* Note that PTR_TO_PHYS only works with 32-bit addresses, but then
   so does the bcm528x. */
#define PTR_TO_PHYS(x) (K0_TO_PHYS((uintptr_t)(x)))
#define PHYS_TO_PTR(a) ((void *)PHYS_TO_K0(a))

/* For the 5821, all mappings through the PCI host bridge use match
   bits mode.  This works because the NORM_PCI bit in DMA Control is
   clear.  The 5820 does not have such a bit, so pointers to data byte
   sequences use match bytes, but control blocks use match bits. */
#define PHYS_TO_PCI(a) ((uint32_t) (a) | 0x20000000)
#define PHYS_TO_PCI_D(a) (a)
#define PCI_TO_PHYS(a) ((uint32_t) (a) & 0x1FFFFFFF)

#define READCSR(sc,csr)      (phys_read32((sc)->regbase + (csr)))
#define WRITECSR(sc,csr,val) (phys_write32((sc)->regbase + (csr), (val)))


static void
dumpcsrs(bcm5821_state_t *sc, const char *legend)
{
    xprintf("%s:\n", legend);
    xprintf("---DMA---\n");
    /* DMA control and status registers */
    xprintf("MCR1: %08X  CTRL: %08X  STAT: %08X  ERR:  %08X\n",
	    READCSR(sc, R_MCR1), READCSR(sc, R_DMA_CTRL), 
	    READCSR(sc, R_DMA_STAT), READCSR(sc, R_DMA_ERR));
    xprintf("MCR2: %08X\n", READCSR(sc, R_MCR2));
    xprintf("-------------\n");
}


static void
bcm5821_init(bcm5821_state_t *sc)
{
}

static void
bcm5821_hwinit(bcm5821_state_t *sc)
{
    uint32_t ctrl;
    uint32_t status;

    ctrl = (M_DMA_CTRL_MCR1_INT_EN | M_DMA_CTRL_MCR2_INT_EN |
	    M_DMA_CTRL_DMAERR_EN);
    if (sc->device == K_PCI_ID_BCM5820)
      ctrl |= (M_DMA_CTRL_NORM_PCI | M_DMA_CTRL_LE_CRYPTO);
    /* Note for 5821: M_DMA_CTRL_NORM_PCI, M_DMA_CTRL_LE_CRYPTO not set. */
    WRITECSR(sc, R_DMA_CTRL, ctrl);

    status = READCSR(sc, R_DMA_STAT);
    WRITECSR(sc, R_DMA_STAT, status);    /* reset write-to-clear bits */
    status = READCSR(sc, R_DMA_STAT);

    dumpcsrs(sc, "init");
}


static void
bcm5821_start(bcm5821_state_t *sc)
{
    bcm5821_hwinit(sc);
}

static void
bcm5821_stop(bcm5821_state_t *sc)
{
    WRITECSR(sc, R_DMA_CTRL, 0);
}


static int bcm5821_open(cfe_devctx_t *ctx);
static int bcm5821_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm5821_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm5821_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm5821_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm5821_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t bcm5821_dispatch = {
    bcm5821_open,
    bcm5821_read,
    bcm5821_inpstat,
    bcm5821_write,
    bcm5821_ioctl,
    bcm5821_close,	
    NULL,
    NULL
};

cfe_driver_t bcm5821drv = {
    "BCM582x crypto",
    "crypt",
    CFE_DEV_OTHER,
    &bcm5821_dispatch,
    bcm5821_probe
};


static int
bcm5821_attach(cfe_driver_t *drv, pcitag_t tag, int index)
{
    bcm5821_state_t *sc;
    char descr[80];
    phys_addr_t pa;
    uint32_t base;
    pcireg_t device, class;

    pci_map_mem(tag, PCI_MAPREG(0), PCI_MATCH_BITS, &pa);
    base = (uint32_t)pa;

    sc = (bcm5821_state_t *) KMALLOC(sizeof(bcm5821_state_t),0);
    if (sc == NULL) {
	xprintf("BCM5821: No memory to complete probe\n");
	return 0;
	}

    memset(sc, 0, sizeof(*sc));

    sc->regbase = base;

    sc->irq = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG) & 0xFF;

    device = pci_conf_read(tag, PCI_ID_REG);
    class = pci_conf_read(tag, PCI_CLASS_REG);

    sc->tag = tag;
    sc->device = PCI_PRODUCT(device);
    sc->revision = PCI_REVISION(class);

    bcm5821_init(sc);

    xsprintf(descr, "BCM%04X Crypto at 0x%08X", sc->device, base);
    cfe_attach(drv, sc, NULL, descr);

    return 1;
}

static void
bcm5821_probe(cfe_driver_t *drv,
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

	if (pci_find_class(PCI_CLASS_PROCESSOR, index, &tag) != 0)
	   break;

	index++;

	device = pci_conf_read(tag, PCI_ID_REG);
	if (PCI_VENDOR(device) == K_PCI_VENDOR_BROADCOM) {
	    if (PCI_PRODUCT(device) == K_PCI_ID_BCM5820 ||
		PCI_PRODUCT(device) == K_PCI_ID_BCM5821) {
		bcm5821_attach(drv, tag, n);
		n++;
		}
	    }
	}
}


/* The functions below are called via the dispatch vector for the 5821 */

static int
bcm5821_open(cfe_devctx_t *ctx)
{
    bcm5821_state_t *sc = ctx->dev_softc;

    bcm5821_start(sc);
    return 0;
}

static int
bcm5821_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    return -1;
}

static int
bcm5821_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
    return 0;
}

static int
bcm5821_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    return -1;
}

static int
bcm5821_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer) 
{
    return -1;
}

static int
bcm5821_close(cfe_devctx_t *ctx)
{
    bcm5821_state_t *sc = ctx->dev_softc;

    bcm5821_stop(sc);
    return 0;
}


/* Additional hooks for testing. */

static int
bcm5821_dump_cc1 (uint32_t *cc)
{
    int  i;
    unsigned op = G_CC_OPCODE(cc[0]);
    unsigned cc_words = G_CC_LEN(cc[0])/4;
    int chain_out;        /* Whether the output is chained or fixed */

    chain_out = 1;        /* default */

    switch (op) {

    case K_SSL_MAC:
      xprintf("(SSL_MAC)\n");
      for (i = 0; i < SSL_MAC_CMD_WORDS; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      chain_out = 0;
      break;

    case K_ARC4:
      xprintf("(ARCFOUR)\n");
      for (i = 0; i < 3; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      for (i = 0; i < 256/4; i += 4)
	xprintf("    %2d: %08x %08x %08x %08x\n",
		i+3, cc[i+3], cc[i+4], cc[i+5], cc[i+6]);
      break;

    case K_HASH:
      xprintf("(HASH)\n");
      for (i = 0; i < 2; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      chain_out = 0;
      break;

    case K_TLS_HMAC:
      chain_out = 0;
      /* fall through */

    default:  /* NYI: K_IPSEC_3DES (5821 only), K_SSL_3DES */
      xprintf("\n");
      for (i = 0; i < cc_words; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      break;
    }

    return chain_out;
}

static int
bcm5821_dump_cc2 (uint32_t *cc)
{
    int  i;
    unsigned op = G_CC_OPCODE(cc[0]);
    unsigned cc_words = G_CC_LEN(cc[0])/4;
    int chain_out;        /* Whether the output is chained or fixed */

    chain_out = 1;        /* default */

    switch (op) {

    case K_RNG_DIRECT:
      xprintf("  RNG_DIRECT\n");
      chain_out = 0;
      for (i = 0; i < 1; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      break;

    case K_RNG_SHA1:
      xprintf("  RNG_SHA1\n");
      chain_out = 0;
      for (i = 0; i < 1; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      break;

    default:  /* NYI: K_DH_*_GEN, K_RSA_*_OP, K_DSA_*, K_MOD_* */
      xprintf("  %04x\n", op);
      for (i = 0; i < cc_words; i++)
	xprintf("    %2d: %08x\n", i, cc[i]);
      break;
    }
    return chain_out;
}

static void
bcm5821_dump_pkt (uint32_t *pkt, int port)
{
    uint32_t *cc = PHYS_TO_PTR(PCI_TO_PHYS(pkt[0]));
    uint32_t *chain;
    int chain_out;
    int i, j;

    xprintf("  %2d: %08x ", 0, pkt[0]);
    chain_out = (port == 1 ? bcm5821_dump_cc1 : bcm5821_dump_cc2)(cc); 

    for (i = 1; i < PD_SIZE/4; i++) {
      xprintf("  %2d: %08x\n", i, pkt[i]);

      if (pkt[i] != 0) {
	switch (i) {
	case 2:
	  chain = PHYS_TO_PTR(PCI_TO_PHYS(pkt[i]));
	  for (j = 0; j < CHAIN_WORDS; j++)
	    xprintf("    %2d: %08x\n", j, chain[j]); 
	  break;
	case 6:
	  if (chain_out) {
	    chain = PHYS_TO_PTR(PCI_TO_PHYS(pkt[i]));
	    for (j = 0; j < CHAIN_WORDS; j++)
	      xprintf("    %2d: %08x\n", j, chain[j]); 
	  }
	  break;
	default:
	  break;
	}
      }
    }
}

static void
bcm5821_dump_mcr (uint32_t mcr[], int port)
{
    unsigned i;
    unsigned npkts = G_MCR_NUM_PACKETS(mcr[0]);
    
    xprintf("MCR header %08x at %p:\n", mcr[0], mcr);
    for (i = 0; i < npkts; i++) {
      xprintf(" packet %d:\n", i+1);
      bcm5821_dump_pkt(&mcr[1 + i*(PD_SIZE/4)], port);
    }
}


static void
bcm5821_show_pkt1 (uint32_t *pkt)
{
    uint32_t *cc = PHYS_TO_PTR(PCI_TO_PHYS(pkt[0]));
    unsigned op = G_CC_OPCODE(cc[0]);
    int i;

    switch (op) {
    case K_SSL_MAC:
      {
	uint32_t *hash = PHYS_TO_PTR(PCI_TO_PHYS(pkt[6]));
	xprintf("SSL_MAC hash:\n");
	xprintf("  %08x %08x %08x %08x\n",
		hash[0], hash[1], hash[2], hash[3]);
	xprintf("  %08x\n", hash[4]);
      }
      break;
    case K_TLS_HMAC:
      {
	uint32_t *hash = PHYS_TO_PTR(PCI_TO_PHYS(pkt[7]));
	xprintf("TLS_HMAC hash:\n");
	xprintf("  %08x %08x %08x %08x\n",
		hash[0], hash[1], hash[2], hash[3]);
	xprintf("  %08x\n", hash[4]);
      }
      break;
    case K_ARC4:
      {
	uint32_t *output = PHYS_TO_PTR(PCI_TO_PHYS(pkt[5]));
	uint32_t *chain =  PHYS_TO_PTR(PCI_TO_PHYS(pkt[6]));
	uint32_t *update = PHYS_TO_PTR(PCI_TO_PHYS(chain[0]));
			 
	xprintf("ARCFOUR output\n");
	for (i = 0; i < 64; i += 4)
	  xprintf (" %08x %08x %08x %08x\n",
		   output[i+0], output[i+1], output[i+2], output[i+3]);
	xprintf("ARCFOUR update\n");
	xprintf(" %08x\n", update[0]);
	for (i = 0; i < 256/4; i += 4)
	  xprintf (" %08x %08x %08x %08x\n",
		   update[i+1], update[i+2], update[i+3], update[i+4]);
      }
      break;
    case K_HASH:
      {
	uint8_t *digest = PHYS_TO_PTR(PCI_TO_PHYS(pkt[6]));

	xprintf("HASH digest ");
	for (i = 0; i < 16; i++)
	  xprintf("%02x", digest[i]);
	xprintf("\n");
      }
      break;
    default:
      break;
    }
}

static void
bcm5821_show_pkt2 (uint32_t *pkt)
{
    uint32_t *cc = PHYS_TO_PTR(PCI_TO_PHYS(pkt[0]));
    unsigned op = G_CC_OPCODE(cc[0]);
    int i;

    switch (op) {
    case K_RNG_DIRECT:
    case K_RNG_SHA1:
      {
	uint32_t *output = PHYS_TO_PTR(PCI_TO_PHYS(pkt[5]));
	size_t len = V_DBC_DATA_LEN(pkt[7])/sizeof(uint32_t);

	xprintf("RNG output\n");
	for (i = 0; i < len; i += 4)
	  xprintf (" %08x %08x %08x %08x\n",
		   output[i+0], output[i+1], output[i+2], output[i+3]);
      }
      break;
    default:
      break;
    }
}

static void
bcm5821_show_mcr (uint32_t mcr[], int port)
{
    unsigned i;
    unsigned npkts = G_MCR_NUM_PACKETS(mcr[0]);
    
    xprintf("MCR at %p:\n", mcr);
    for (i = 0; i < npkts; i++) {
      xprintf("packet %d:\n", i+1);
      if (port == 1)
	bcm5821_show_pkt1(&mcr[1 + i*(PD_SIZE/4)]);
      else
	bcm5821_show_pkt2(&mcr[1 + i*(PD_SIZE/4)]);
    }
}


static uint32_t *
bcm5821_alloc_hash (const uint8_t *msg, size_t msg_len, int swap)
{
    uint32_t *mcr;
    uint32_t *cmd;    /* always reads at least 64 bytes */
    uint8_t  *message;
    uint8_t  *digest;
    int i;

    message = KMALLOC(msg_len, CACHE_LINE_SIZE);
    for (i = 0; i < msg_len; i++)
      message[i] = msg[i];
   
    digest = KMALLOC(16, CACHE_LINE_SIZE);
    for (i = 0; i < 16; i++)
      digest[i] = 0;
   
    mcr = KMALLOC(MCR_WORDS(1)*4, CACHE_LINE_SIZE);
    mcr[0] = V_MCR_NUM_PACKETS(1);

    cmd = KMALLOC(64, CACHE_LINE_SIZE);   /* Always allocate >= 64 bytes */
    cmd[0] = V_CC_OPCODE(K_HASH) | V_CC_LEN(8);
    cmd[1] = V_CC_FLAGS(K_HASH_FLAGS_MD5);

    mcr[1] = PHYS_TO_PCI(PTR_TO_PHYS(cmd));

    /* input fragment */
    mcr[2] = swap ? PHYS_TO_PCI_D(PTR_TO_PHYS(message))
                  : PHYS_TO_PCI(PTR_TO_PHYS(message));
    mcr[3] = 0;
    mcr[4] = V_DBC_DATA_LEN(msg_len);

    mcr[5] = V_PD_PKT_LEN(msg_len);

    mcr[6] = 0;
    mcr[7] = swap ? PHYS_TO_PCI_D(PTR_TO_PHYS(digest))
                  : PHYS_TO_PCI(PTR_TO_PHYS(digest));
    mcr[8] = 0;

    return mcr;
}

static void
bcm5821_free_hash (uint32_t mcr[])
{
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[1])));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[2])));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[7])));

    KFREE(mcr);
}


static uint32_t *
bcm5821_alloc_hmac (const char *key, int key_len,
		    const char *msg, int msg_len,
		    int swap)
{
    uint32_t *message;
    uint32_t *cmd;
    uint32_t *mcr;
    uint32_t *hash;
    int i;

    message = KMALLOC(msg_len, CACHE_LINE_SIZE);
    memcpy((uint8_t *)message, msg, msg_len);
   
    mcr = KMALLOC(MCR_WORDS(1)*4, CACHE_LINE_SIZE);
    mcr[0] = V_MCR_NUM_PACKETS(1);

    /* packet 1 */

    cmd = KMALLOC(TLS_HMAC_CMD_WORDS*4, CACHE_LINE_SIZE);
    cmd[0] = V_CC_OPCODE(K_TLS_HMAC) | V_CC_LEN(TLS_HMAC_CMD_WORDS*4);
    cmd[1] = V_CC_FLAGS(K_HASH_FLAGS_MD5);

    for (i = 2; i < 7; i++)
      cmd[i] = 0x36363636;
    cmd[6] = 0x00000000;      /* must be zero for SSL */
    for (i = 8; i < 13; i++)
      cmd[i] = 0x5c5c5c5c;
    cmd[13] = 0;              /* seq num */
    cmd[14] = 1;
    cmd[15] = 0x03000000 | (msg_len << 8);

    mcr[1] = PHYS_TO_PCI(PTR_TO_PHYS(cmd));

    /* input fragment */
    mcr[2] = swap ? PHYS_TO_PCI_D(PTR_TO_PHYS(message))
                  : PHYS_TO_PCI(PTR_TO_PHYS(message));
    mcr[3] = 0;
    mcr[4] = V_DBC_DATA_LEN(msg_len);

    mcr[5] = V_PD_PKT_LEN(msg_len);

    hash = KMALLOC(5*4, CACHE_LINE_SIZE);
    for (i = 0; i < 5; i++)
      hash[i] = 0;
   
    mcr[6] = 0;
    mcr[7] = swap ? PHYS_TO_PCI_D(PTR_TO_PHYS(hash))
                  : PHYS_TO_PCI(PTR_TO_PHYS(hash));
    mcr[8] = 0;

    return mcr;
}

static void
bcm5821_free_hmac (uint32_t mcr[])
{
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[1])));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[2])));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[7])));

    KFREE(mcr);
}


static int test_init = 0;

/* Timing */

#if _SB1250_PERF_
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

#else /* !_SB1250_PERF_ */
static void
zclk_init(uint64_t val)
{
}

static uint64_t
zclk_get(void)
{
    return 0;
}

static uint64_t
zclk_elapsed(uint64_t stop, uint64_t start)
{
    return 0;
}
#endif /* _SB1250_PERF_ */


/* Auxiliary functions */

static uint32_t *
bcm5821_alloc_composite(int input_size)
{
    uint32_t *input, *output;
    uint32_t *cmd;
    uint32_t *chain;
    uint32_t *mcr;
    uint32_t *hash;
    uint32_t *update;
    uint8_t *arc4_state;
    int i;

    input = KMALLOC(input_size, CACHE_LINE_SIZE);
    for (i = 0; i < input_size; i++)
      ((uint8_t *)input)[i] = i & 0xFF;
    output = KMALLOC(input_size + 16, CACHE_LINE_SIZE);
    for (i = 0; i < input_size + 16; i++)
      ((uint8_t *)output)[i] = 0xFF;
    
    mcr = KMALLOC(MCR_WORDS(2)*4, CACHE_LINE_SIZE);
    mcr[0] = V_MCR_NUM_PACKETS(2);

    /* packet 1 */

    cmd = KMALLOC(SSL_MAC_CMD_WORDS*4, CACHE_LINE_SIZE);
    cmd[0] = V_CC_OPCODE(K_SSL_MAC) | V_CC_LEN(SSL_MAC_CMD_WORDS*4);
    cmd[1] = V_CC_FLAGS(K_HASH_FLAGS_MD5);
    for (i = 2; i < 6; i++)
      cmd[i] = 0x01020304;
    cmd[6] = 0x00000000;      /* must be zero for SSL */
    for (i = 7; i < 19; i++)
      cmd[i] = 0x36363636;
    cmd[19] = 0;              /* seq num */
    cmd[20] = 1;
    cmd[21] = 0x03000000 | (input_size << 8);   /* type/len/rsvd */

    mcr[1] = PHYS_TO_PCI(PTR_TO_PHYS(cmd));

    /* input fragment */
    mcr[2] = PHYS_TO_PCI(PTR_TO_PHYS(input));
    mcr[3] = 0;
    mcr[4] = V_DBC_DATA_LEN(input_size);

    mcr[5] = V_PD_PKT_LEN(input_size);

    hash = KMALLOC(5*4, CACHE_LINE_SIZE);
    for (i = 0; i < 5; i++)
      hash[i] = 0;
   
    mcr[6] = 0;
    mcr[7] = PHYS_TO_PCI(PTR_TO_PHYS(hash));
    mcr[8] = 0;

    /* packet 2 */

    cmd = KMALLOC(ARC4_CMD_WORDS*4, CACHE_LINE_SIZE);
    cmd[0] = V_CC_OPCODE(K_ARC4) | V_CC_LEN(ARC4_CMD_WORDS*4);
    cmd[1] = M_ARC4_FLAGS_WRITEBACK;
    cmd[2] = 0x000100F3;
    arc4_state = (uint8_t *)&cmd[3];
    for (i = 0; i < 256; i++)
      arc4_state[i] = i;

    mcr[8+1] = PHYS_TO_PCI(PTR_TO_PHYS(cmd));

    /* input fragment */
    chain = KMALLOC(CHAIN_WORDS*4, CACHE_LINE_SIZE);

    mcr[8+2] = PHYS_TO_PCI(PTR_TO_PHYS(input));
    mcr[8+3] = PHYS_TO_PCI(PTR_TO_PHYS(chain));
    mcr[8+4] = V_DBC_DATA_LEN(input_size);

    /* MAC fragment */
    chain[0] = PHYS_TO_PCI(PTR_TO_PHYS(hash));
    chain[1] = 0;
    chain[2] = V_DBC_DATA_LEN(16);

    mcr[8+5] = V_PD_PKT_LEN(input_size + 16);

    /* output fragment */
    chain = KMALLOC(CHAIN_WORDS*4, CACHE_LINE_SIZE);

    mcr[8+6] = PHYS_TO_PCI(PTR_TO_PHYS(output));
    mcr[8+7] = PHYS_TO_PCI(PTR_TO_PHYS(chain));
    mcr[8+8] = V_DBC_DATA_LEN(input_size + 16);

    update = KMALLOC(ARC4_STATE_WORDS*4, CACHE_LINE_SIZE);
    for (i = 0; i < ARC4_STATE_WORDS; i++)
      update[i] = 0xFFFFFFFF;
   
    /* output update */
    chain[0] = PHYS_TO_PCI(PTR_TO_PHYS(update));
    chain[1] = 0;
    chain[2] = V_DBC_DATA_LEN(ARC4_STATE_WORDS*4);  /* not actually used */

    return mcr;
}

static void
bcm5821_free_composite (uint32_t mcr[])
{
    uint32_t *chain;

    /* packet 1 */

    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[1])));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[2])));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[7])));

    /* packet 2 */
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[8+1])));
    /* mcr[8+2] already freed */
    chain = PHYS_TO_PTR(PCI_TO_PHYS(mcr[8+3]));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(chain[0])));  KFREE(chain);
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(mcr[8+6])));
    chain = PHYS_TO_PTR(PCI_TO_PHYS(mcr[8+7]));
    KFREE(PHYS_TO_PTR(PCI_TO_PHYS(chain[0])));  KFREE(chain);

    KFREE(mcr);
}


static void
flush_l2(void)
{
  /* Temporary hack: churn through all of L2 */
  volatile uint64_t *lomem;
  uint64_t t;
  int i;

  lomem = (uint64_t *)(0xFFFFFFFF80000000LL);   /* kseg0 @ 0 */
  t = 0;
  for (i = 0; i < (512/8)*1024; i++)
    t ^= lomem[i];
}

#ifdef IRQ
static void
bcm5821_interrupt(void *ctx)
{
}
#endif


#define POOL_SIZE       4
#define MCR_QUEUE_DEPTH 2
  
static int
bcm5821_composite (bcm5821_state_t *sc, size_t len, int trials)
{
    uint32_t *mcr[POOL_SIZE];
    uint32_t status;
    uint64_t start, stop, ticks;
    uint64_t tpb, Mbs;
    int i;
    int next, last, run;

    for (i = 0; i < POOL_SIZE; i++)
      mcr[i] = bcm5821_alloc_composite(len);

    (void)bcm5821_dump_mcr;  /*bcm5821_dump_mcr(mcr[0], 1);*/

    next = last = 0;
    run = 0;

    /* Force all descriptors and buffers out of L1 */
    cfe_flushcache(CFE_CACHE_FLUSH_D);
    (void)flush_l2;

    status = READCSR(sc, R_DMA_STAT);
    WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */
    status = READCSR(sc, R_DMA_STAT);

    for (i = 0; i < 1000; i++) {
      status = READCSR(sc, R_DMA_STAT);
      if ((status & M_DMA_STAT_MCR1_FULL) == 0)
	break;
      cfe_sleep(1);
    }
    if (i == 1000) {
      dumpcsrs(sc, "bcm5821: full bit never clears");
      return -1;
    }

#ifdef IRQ
    /* Enable interrupt polling, but the handler is never called. */
    cfe_request_irq(sc->irq, bcm5821_interrupt, NULL, 0, 0);
#endif

    zclk_init(0);     /* Time origin is arbitrary. */
    start = zclk_get();

    /* MCR ports are double buffered. */
    for (i = 0; i < MCR_QUEUE_DEPTH; i++) {
      while ((READCSR(sc, R_DMA_STAT) & M_DMA_STAT_MCR1_FULL) != 0)
	continue;
      WRITECSR(sc, R_MCR1, PHYS_TO_PCI(PTR_TO_PHYS(mcr[next])));
      next = (next + 1) % POOL_SIZE;
    }

    while (1) {
#ifdef IRQ
      while ((_getcause() & M_CAUSE_IP2) == 0)
	continue;

      status = READCSR(sc, R_DMA_STAT);
      if ((status & M_DMA_STAT_MCR1_INTR) == 0) {
	/* This apparently is MCR1_ALL_EMPTY, timing of which is unclear. */
	WRITECSR(sc, R_DMA_STAT,
		 M_DMA_STAT_DMAERR_INTR | M_DMA_STAT_MCR1_INTR);
	continue;
      }

      stop = zclk_get();
      WRITECSR(sc, R_DMA_STAT,
	       M_DMA_STAT_DMAERR_INTR | M_DMA_STAT_MCR1_INTR);
#else
      volatile uint32_t *last_mcr = mcr[last];

      while ((*last_mcr & M_MCR_DONE) == 0)
	continue;

      stop = zclk_get();
#endif

      run++;
      if (run == trials)
	break;

      while ((READCSR(sc, R_DMA_STAT) & M_DMA_STAT_MCR1_FULL) != 0)
	continue;
      WRITECSR(sc, R_MCR1, PHYS_TO_PCI(PTR_TO_PHYS(mcr[next])));
      next = (next + 1) % POOL_SIZE;

      /* Clear the DONE and ERROR bits.  This will bring one line of
         the MCR back into L1.  Flush? */
      mcr[last][0] = V_MCR_NUM_PACKETS(2);
      last = (last + 1) % POOL_SIZE;
    }

#ifdef IRQ
    status = READCSR(sc, R_DMA_STAT);
    WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */
    cfe_free_irq(sc->irq, 0);
#endif

    ticks = zclk_elapsed(stop, start) / trials;
    xprintf("bcm5821: Composite %lld ticks for %d bytes, %d packets\n",
	    ticks, len, trials);
    /* Scaling for two decimal places. */
    tpb = (ticks*100) / len;
    Mbs = (2000*100)*100 / tpb;
    xprintf("  rate %lld.%02lld Mbps\n", Mbs/100, Mbs % 100);

    if (trials == 1)
      {
	bcm5821_show_mcr(mcr[0], 1);
      }

    for (i = 0; i < POOL_SIZE; i++)
      bcm5821_free_composite(mcr[i]);

    return 0;
}


/* The following code depends on having a separate interrupt per
   device, and there are only 4 PCI interrupts. */
#define MAX_DEVICES 4

struct dev_info {
    bcm5821_state_t *sc;
    uint64_t         irq_mask;
    int              index[MCR_QUEUE_DEPTH];
};
  
  
#define N_DEVICES  2

static int
bcm5821_composite2 (bcm5821_state_t *sc0, bcm5821_state_t *sc1,
		    size_t len, int trials)
{
    uint32_t *mcr[POOL_SIZE];
    uint32_t ring[POOL_SIZE];
    uint32_t status;
    uint64_t start, stop, ticks;
    uint64_t tpb, Mbs;
    int i;
    int next, last;
    int started, run;
    int d;
    struct dev_info dev[N_DEVICES];
    uint64_t masks;
    bcm5821_state_t *sc;
#ifdef IRQ
    volatile uint64_t *irqstat = IMR_POINTER(0, R_IMR_INTERRUPT_SOURCE_STATUS);
#endif
    uint64_t pending;

    dev[0].sc = sc0;  dev[1].sc = sc1;

    for (i = 0; i < POOL_SIZE; i++)
      mcr[i] = bcm5821_alloc_composite(len);
    for (i = 0; i < POOL_SIZE; i++)
      ring[i] = i;
    next = last = 0;

    (void)bcm5821_dump_mcr;  /*bcm5821_dump_mcr(mcr[0], 1);*/

    started = run = 0;

    /* Force all descriptors and buffers out of L1 */
    cfe_flushcache(CFE_CACHE_FLUSH_D);
    (void)flush_l2;

    masks = 0;
    for (d = 0; d < N_DEVICES; d++) {
      sc = dev[d].sc;
      dev[d].irq_mask = 1LL << (sc->irq);
      masks |= dev[d].irq_mask;

      status = READCSR(sc, R_DMA_STAT);
      WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */
      status = READCSR(sc, R_DMA_STAT);

      for (i = 0; i < 1000; i++) {
	status = READCSR(sc, R_DMA_STAT);
	if ((status & M_DMA_STAT_MCR1_FULL) == 0)
	  break;
	cfe_sleep(1);
      }

      if (i == 1000) {
	dumpcsrs(sc, "bcm5821: full bit never clears");
	return -1;
      }

#ifdef IRQ
      /* Enable interrupt polling, but the handler is never called. */
      cfe_request_irq(sc->irq, bcm5821_interrupt, NULL, 0, 0);
#endif
    }

    stop = 0;         /* Keep compiler happy */
    zclk_init(0);     /* Time origin is arbitrary. */
    start = zclk_get();

    for (d = 0; d < N_DEVICES; d++) {
      sc = dev[d].sc;

      /* MCR ports are double buffered. */
      for (i = 0; i < 2; i++) {
	int index = ring[next];
	while ((READCSR(sc, R_DMA_STAT) & M_DMA_STAT_MCR1_FULL) != 0)
	  continue;
	WRITECSR(sc, R_MCR1, PHYS_TO_PCI(PTR_TO_PHYS(mcr[index])));
	dev[d].index[i] = index;
	next = (next + 1) % POOL_SIZE;
	started++;
      }
    }

    while (trials == 0 || run != trials) {
#ifdef IRQ
      while ((_getcause() & M_CAUSE_IP2) == 0)
	continue;

      pending = *irqstat;
#else
      pending = 0;
      while (pending == 0) {
	for (d = 0; d < N_DEVICES; d++) {
	  volatile uint32_t *last_mcr = mcr[dev[d].index[0]];

	  if ((*last_mcr & M_MCR_DONE) != 0)
	    pending |= dev[d].irq_mask;
	}
      }
#endif

      stop = zclk_get();

      for (d = 0; d < N_DEVICES; d++) {
	if ((dev[d].irq_mask & pending) != 0) {
	  sc = dev[d].sc;

#ifdef IRQ
          status = READCSR(sc, R_DMA_STAT);          
	  if ((status & M_DMA_STAT_MCR1_INTR) == 0) {
	    /* Apparently MCR1_ALL_EMPTY, timing of which is unclear. */
	    WRITECSR(sc, R_DMA_STAT,
		     M_DMA_STAT_DMAERR_INTR | M_DMA_STAT_MCR1_INTR);
	    continue;
	  }
	  WRITECSR(sc, R_DMA_STAT,
		   M_DMA_STAT_DMAERR_INTR | M_DMA_STAT_MCR1_INTR);
#endif
	  ring[last] = dev[d].index[0];
	  /* Clear the DONE and ERROR bits.  This will bring one line of
	     the MCR back into L1.  Flush? */
	  mcr[ring[last]][0] = V_MCR_NUM_PACKETS(2);
	  last = (last + 1) % POOL_SIZE;

	  run++;
	  if (run == trials)
	    break;

	  dev[d].index[0] = dev[d].index[1];
	  if (trials == 0 || started < trials) {
	    int index = ring[next];
	    while ((READCSR(sc, R_DMA_STAT) & M_DMA_STAT_MCR1_FULL) != 0)
	      continue;
	    WRITECSR(sc, R_MCR1, PHYS_TO_PCI(PTR_TO_PHYS(mcr[index])));
	    dev[d].index[1] = index;
	    next = (next + 1) % POOL_SIZE;
	    started++;
	  }
	}
      }
    }

    for (d = 0; d < N_DEVICES; d++) {
      sc = dev[d].sc;
      status = READCSR(sc, R_DMA_STAT);
      WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */
#ifdef IRQ
      cfe_free_irq(sc->irq, 0);
#endif
    }

    ticks = zclk_elapsed(stop, start) / trials;
    xprintf("bcm5821: Composite %lld ticks for %d bytes, %d packets\n",
	    ticks, len, trials);
    /* Scaling for two decimal places. */
    tpb = (ticks*100) / len;
    Mbs = (2000*100)*100 / tpb;
    xprintf("  rate %lld.%02lld Mbps\n", Mbs/100, Mbs % 100);

    for (i = 0; i < POOL_SIZE; i++)
      bcm5821_free_composite(mcr[i]);

    return 0;
}


extern cfe_devctx_t *cfe_handle_table[];

int bcm5821_test (int device, int trials);
int
bcm5821_test (int device, int trials)
{
    cfe_devctx_t *ctx = cfe_handle_table[device];
    bcm5821_state_t *sc = ctx->dev_softc;

    if (!test_init) {
      zclk_init(0);     /* Time origin is arbitrary */
      test_init = 1;
    }

    bcm5821_composite(sc, 1472, trials);

    return 0;
}

int bcm5821_test2 (int device0, int device2, int trials);
int
bcm5821_test2 (int device0, int device1, int trials)
{
    cfe_devctx_t *ctx0 = cfe_handle_table[device0];
    cfe_devctx_t *ctx1 = cfe_handle_table[device1];
    bcm5821_state_t *sc0 = ctx0->dev_softc;
    bcm5821_state_t *sc1 = ctx1->dev_softc;

    if (!test_init) {
      zclk_init(0);     /* Time origin is arbitrary */
      test_init = 1;
    }

    bcm5821_composite2(sc0, sc1, 1472, trials);

    return 0;
}


static int
bcm5821_hash_md5 (bcm5821_state_t *sc, const char *msg)
{
    size_t len = strlen(msg);
    uint32_t *mcr;
    uint32_t status;
    int i;
    int swap = (sc->device == K_PCI_ID_BCM5820);

    mcr = bcm5821_alloc_hash(msg, len, swap);

    /* bcm5821_dump_mcr(mcr, 1); */

    status = READCSR(sc, R_DMA_STAT);
    WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */
    status = READCSR(sc, R_DMA_STAT);

    for (i = 0; i < 1000; i++) {
      status = READCSR(sc, R_DMA_STAT);
      if ((status & M_DMA_STAT_MCR1_FULL) == 0)
	break;
      cfe_sleep(1);
    }
    if (i == 1000) {
      dumpcsrs(sc, "bcm5821: full bit never clears");
      return -1;
    }

    WRITECSR(sc, R_MCR1, PHYS_TO_PCI(PTR_TO_PHYS(mcr)));

    for (i = 0; i < 1000; i++) {
#ifdef IRQ
      status = READCSR(sc, R_DMA_STAT);
      if ((status & M_DMA_STAT_MCR1_INTR) != 0)
	break;
#else
      if ((mcr[0] & M_MCR_DONE) != 0)
	break;
#endif
      cfe_sleep(1);
    }
    if (i == 1000) {
      dumpcsrs(sc, "bcm5821: done bit never sets");
      /*return -1;*/
    }

    status = READCSR(sc, R_DMA_STAT);
    WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */

    /* bcm5821_dump_mcr(mcr, 1); */

    bcm5821_show_mcr(mcr, 1);

    bcm5821_free_hash(mcr);

    return 0;
}


static int
bcm5821_hmac_md5 (bcm5821_state_t *sc,
		  const uint8_t key[],  size_t key_len,
		  const uint8_t data[], size_t data_len)
{
    uint32_t *mcr;
    uint32_t status;
    int i;
    int swap = (sc->device == K_PCI_ID_BCM5820);

    mcr = bcm5821_alloc_hmac(key, key_len, data, data_len, swap);

    status = READCSR(sc, R_DMA_STAT);
    WRITECSR(sc, R_DMA_STAT, status);    /* clear pending bits */
    status = READCSR(sc, R_DMA_STAT);

    for (i = 0; i < 1000; i++) {
      status = READCSR(sc, R_DMA_STAT);
      if ((status & M_DMA_STAT_MCR1_FULL) == 0)
	break;
      cfe_sleep(1);
    }
    if (i == 1000) {
      dumpcsrs(sc, "bcm5821: full bit never clears");
      return -1;
    }

    bcm5821_free_hmac(mcr);
    return 0;
}

/* Sanity check on the implementation using RFC test suites. */

int bcm5821_check (int device);
int
bcm5821_check (int device)
{
    static unsigned char k1[16] = {
      0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
      0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
    };
    static unsigned char m1[] = "Hi There";

    static unsigned char k2[] = "Jefe";
    static unsigned char m2[] = "what do ya want for nothing?";

    static unsigned char k3[16] = {
      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
    };
    static unsigned char m3[50] = {
      0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
      0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
      0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
      0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
      0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD
    };

    cfe_devctx_t *ctx = cfe_handle_table[device];
    bcm5821_state_t *sc = ctx->dev_softc;
    
    if (!test_init) {
      zclk_init(0);     /* Time origin is arbitrary */
      test_init = 1;
    }

    bcm5821_hash_md5(sc, "a");
    bcm5821_hash_md5(sc, "abc");
    bcm5821_hash_md5(sc, "message digest");
    bcm5821_hash_md5(sc, "abcdefghijklmnopqrstuvwxyz");
    bcm5821_hash_md5(sc, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    bcm5821_hash_md5(sc, "12345678901234567890123456789012345678901234567890123456789012345678901234567890");
    
    bcm5821_hmac_md5(sc, k1, sizeof(k1), m1, strlen(m1));
    bcm5821_hmac_md5(sc, k2, strlen(k2), m2, strlen(m2));
    bcm5821_hmac_md5(sc, k3, sizeof(k3), m3, sizeof(m3));

    return 0;
}

/* Output of md5 test suite (md5 -x)

MD5 test suite:
MD5 ("") = d41d8cd98f00b204e9800998ecf8427e
MD5 ("a") = 0cc175b9c0f1b6a831c399e269772661
MD5 ("abc") = 900150983cd24fb0d6963f7d28e17f72
MD5 ("message digest") = f96b697d7cb7938d525a2f31aaf161d0
MD5 ("abcdefghijklmnopqrstuvwxyz") = c3fcd3d76192e4007dfb496cca67e13b
MD5 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = d174ab98d277d9f5a5611c2c9f419d9f
MD5 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = 57edf4a22be3c955ac49da2e2107b67a

*/

/* HMAC-MD5 test suite

Test Vectors (Trailing '\0' of a character string not included in test):

  key =         0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b
  key_len =     16 bytes
  data =        "Hi There"
  data_len =    8  bytes
  digest =      0x9294727a3638bb1c13f48ef8158bfc9d

  key =         "Jefe"
  data =        "what do ya want for nothing?"
  data_len =    28 bytes
  digest =      0x750c783e6ab0b503eaa86e310a5db738

  key =         0xAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
  key_len       16 bytes
  data =        0xDDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD
  data_len =    50 bytes
  digest =      0x56be34521d144c88dbb8c733f0e8b3f6

*/
