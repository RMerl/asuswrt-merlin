/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
//**************************************************************************
// File Name  : bcm6352Enet.c
//
// Description: This is the CFE network driver for the BCM6352 Ethenet Switch
//               
// Updates    : 09/26/2001  jefchiao.  Created.
//              05/21/2002  lat.       Port to CFE.
//**************************************************************************

#define CARDNAME    "BCM6352_ENET"
#define VERSION     "0.1"
#define VER_STR     "v" VERSION " " __DATE__ " " __TIME__

/* Includes. */
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_ioctl.h"
#include "cfe_device.h"
#include "cfe_devfuncs.h"
#include "sbmips.h"
#include "board.h"
#include "dev_bcm635x_enet.h"
#include "ethersw.h"
#include "dev_bcm63xx_flash.h"

/* Externs. */
extern cfe_driver_t flashdrv;
extern void udelay(long us);
extern NVRAM_DATA nvramData;

/* Prototypes. */
static void bcm6352_enet_probe( cfe_driver_t *drv, unsigned long probe_a,
    unsigned long probe_b, void *probe_ptr );
static void ext_mii_check(void);
static void port_control(int portId, int enable);
static void init_arl(void);
static void init_mii(bcm6352enet_softc *softc);
static void init_dma(bcm6352enet_softc *softc);
static int bcm6352_init_dev(bcm6352enet_softc *softc);
int internal_open( bcm6352enet_softc *softc );
void write_mac_address( bcm6352enet_softc *softc );
static int bcm6352_enet_open(cfe_devctx_t *ctx);
static int bcm6352_enet_read( cfe_devctx_t *ctx, iocb_buffer_t *buffer );
static int bcm6352_enet_inpstat( cfe_devctx_t *ctx, iocb_inpstat_t *inpstat );
static int bcm6352_enet_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm6352_enet_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm6352_enet_close(cfe_devctx_t *ctx);

/* Globals. */
volatile uint32_t *swreg32 = (uint32_t *) ESWITCH_BASE;  
volatile uint16_t *swreg16 = (uint16_t *) ESWITCH_BASE;
volatile unsigned char *swreg8 = (unsigned char *) ESWITCH_BASE;
volatile BusBridge *bridge = (BusBridge *) BB_BASE;

const static cfe_devdisp_t bcm6352_enet_dispatch =
{
    bcm6352_enet_open,
    bcm6352_enet_read,
    bcm6352_enet_inpstat,
    bcm6352_enet_write,
    bcm6352_enet_ioctl,
    bcm6352_enet_close,
    NULL,
    NULL
};

const cfe_driver_t bcm6352_enet =
{
    "BCM6352 Ethernet",
    "eth",
    CFE_DEV_NETWORK,
    &bcm6352_enet_dispatch,
    bcm6352_enet_probe
};


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_probe
 Purpose: Initial call to this driver.
-------------------------------------------------------------------------- */
static void bcm6352_enet_probe( cfe_driver_t *drv, unsigned long probe_a,
    unsigned long probe_b, void * probe_ptr )
{
    bcm6352enet_softc *softc;
    char descr[100];

    softc = (bcm6352enet_softc *) KMALLOC( sizeof(bcm6352enet_softc), 0 );
    if( softc == NULL )
        xprintf( "BCM6352 : Failed to allocate softc memory.\n" );
    else
    {
        memset( softc, 0, sizeof(bcm6352enet_softc) );

        if (internal_open( softc ) == -1) 
            xprintf("Failed initializing enet hardware\n");
        else
        {
            cfe_attach( drv, softc, NULL, descr );
            cfe_attach(&flashdrv, softc, NULL, NULL);
        }
    }
}


/*
 * ext_mii_check - checking external MII status, update MII port state override
 *               register if link up/down, duplex, or speed change.
 */
static void ext_mii_check(void)
{
    int change;
    uint16_t miiaddr0a;
    uint16_t miiaddr32;
    uint16_t miiaddr30;
    uint16_t data16bit;
    uint16_t transtimer;
    uint16_t status = 0;

    transtimer = bridge->status;
    bridge->status = transtimer & ~0x3FF;
    /* read auto-negotiation link partner ability */
    miiaddr0a = swreg16[(PORT1_MII_dp+ANLPA_d)/2];
    udelay(10);

    /* read auxiliary status summary */
    miiaddr32 = swreg16[(PORT1_MII_dp+ASTSSUM_d)/2];
    udelay(10);

    /* read auxiliary control/status */
    miiaddr30 = swreg16[(PORT1_MII_dp+ACTLSTS_d)/2];
    udelay(10);

    /* enable ISB abort timer */
    transtimer = bridge->status;
    bridge->status = transtimer | 0x3FF;

    /* update link partner has pasue capability */
    status |= ((miiaddr0a & 0x0400) != 0 ? 0x0008 : 0);
    /* update current speed indication */
    status |= ((miiaddr32 & 0x0008) != 0 ? 0 : 0x0004);
    /* update current link status */
    status |= ((miiaddr32 & 0x0004) != 0 ? 0x0001 : 0);
    /* update current duplex indication */
    status |= ((miiaddr30 & 0x0001) != 0 ? 0x0002 : 0);


    data16bit = swreg16[(CTLREG_dp+MII_PSTS_d)/2];
    data16bit &= 0x000F;
    change = (data16bit ^ status) != 0 ? 1 : 0;
    if (change) {
        udelay(10);
        /* set MII port state override register for port 1 (external PHY) */
        swreg16[(CTLREG_dp+MII_PSTS_d)/2] = 0xfff0 | status;
    }
}


/*
 * port_control: enable/disable rx functions of the port at the MAC level
 */
static void port_control(int portId, int enable)
{
    unsigned char data8bit;
    int addr;

    switch(portId) {
    case 0:
        addr = TH_PCTL0_d;
        break;
    case 1:
        addr = TH_PCTL1_d;
        break;
    default:
        return;
    }

    /*
     * disable rx function.
     * if disable tx function, it has potential race condition if DMA still
     * has frames need to sending out and it will block the DMA.
     */
    data8bit = swreg8[(CTLREG_dp + addr)];
    if (enable)
        data8bit &= ~RX_DISABLE; // clear RX_DISABLE bit
    else
        data8bit |= RX_DISABLE; // set RX_DISABLE bit
    swreg8[(CTLREG_dp + addr)] = data8bit;
}


/*
 * init_arl: initialise ARL table memory
 *
 * Parameter: None
 *
 * Note:
 *
 *   There are 1K entries in the arl table. However, they are not
 *   physically located consecutively. Each arl entry is a 64-bit
 *   word. It takes 10 bits to address 1K entries. The addressing
 *   scheme is as follows :
 * 
 *   Logical address : addr[9:0]
 * 
 *   Physical address : { addr[9:5], 3'b111, addr[4:0] }
 * 
 *   Please note that the addresses above are 64-bit word addresses,
 *   not byte addresses.
 *
 *   Pattern for initializing table is:
 * 
 *   swreg32[(MEM_dp+MEMDAT_d)/4] = 0x0;       (make DAT = 0)
 *   swreg32[(MEM_dp+MEMDAT_d+4)/4] = 0x0;
 *   
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x800e0; 
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x800e1;
 *   .... 
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x800fe;
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x800ff;  (32 entries )
 * 
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x801e0; 
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x801e1; 
 *   .... 
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x801fe;
 *   swreg32[(MEM_dp+MEMADR_d)/4] = 0x801ff;  (another 32 entries) 
 *   ....
 */
static void init_arl(void)
{
    int bits_9_5;
    int bits_4_0;

    swreg32[(MEM_dp+MEMDAT_d)/4] = 0x0;     /*make DAT = 0*/
    swreg32[(MEM_dp+MEMDAT_d)/4 + 1] = 0x0;

    for ( bits_9_5 = 0x0; bits_9_5 <= 0x1f; bits_9_5++ ) {
        for ( bits_4_0 = 0x0; bits_4_0 <= 0x1f; bits_4_0++ ) {
            swreg32[(MEM_dp+MEMADR_d)/4] = 0x800e0 | bits_9_5 << 8 | bits_4_0;
        }
    }
}


/*
 * init_mii: Initializes the Ethernet Switch control registers
 */
static void init_mii(bcm6352enet_softc *softc)
{
    uint16_t transtimer;
    uint16_t data16bit;

    /* initialize the memeory */
    init_arl();     /* clear ARL table */

   /* set up SMP as the manamgement port */
    swreg32[(MNGMODE_dp+GMNGCFG_d)/4] = SMP_PORT;
    swreg32[(MNGMODE_dp+MNGPID_d)/4] = SMP_PORT_ID;

    /* enable software forwarding */
    if (test_bit(0, &softc->dualMacMode)) {
        swreg32[(CTLREG_dp+TH_PCTL0_d)/4] = FORWARDING_STATE | RX_DISABLE;
        swreg32[(CTLREG_dp+TH_PCTL1_d)/4] = FORWARDING_STATE | RX_DISABLE;
    } else {
        swreg32[(CTLREG_dp+TH_PCTL0_d)/4] = FORWARDING_STATE;
        swreg32[(CTLREG_dp+TH_PCTL1_d)/4] = FORWARDING_STATE;
    }
    swreg32[(CTLREG_dp+SWMODE_d)/4] = SW_FWDG_EN|MANAGED_MODE;

    /* set the MIB autocast header length */
    swreg32[(MIBACST_dp+ACSTHDLT_d)/4] = 0x5555;

    /* enable broadcast packets */
    swreg32[(CTLREG_dp+SMP_CTL_d) /4] = 0x04;

    /* correct the default congestion registers to allow 10BT mode
    * (the defaults will be fixed in B0) */
    swreg16[(CNGMREG_dp + 0x02) /2] = 0x0408;
    swreg16[(CNGMREG_dp + 0x04) /2] = 0x000D;
    swreg16[(CNGMREG_dp + 0x06) /2] = 0x070A;
    swreg16[(CNGMREG_dp + 0x08) /2] = 0x000D;
    swreg16[(CNGMREG_dp + 0x0e) /2] = 0x0C12;
    swreg16[(CNGMREG_dp + 0x10) /2] = 0x001D;

    // disable ISB abort timer for MII port 1 access
    // from/to external Ethernet PHY, the transaction will
    // terminate prematurely due to an early timeout.
    transtimer = bridge->status;
    bridge->status = transtimer & ~0x3FF;
    transtimer = bridge->status;

    // set MII Auxiliary Mode 2 register for port 1 (external PHY)
    // set Activity/Link LED Mode bit for external PHY LED goes active
    // upon acquiring link and pulses during receive or transmit activity
    data16bit = swreg16[(PORT1_MII_dp+AMODE2_d)/2];
    data16bit |= 0x0004;
    udelay(10);
    swreg16[(PORT1_MII_dp+AMODE2_d)/2] = data16bit;
    udelay(10);

    /* advertise pause capability on MII port 1 */
    data16bit = swreg16[(PORT1_MII_dp+ANADV_d)/2];
    data16bit |= 0x0400;
    udelay(10);
    swreg16[(PORT1_MII_dp+ANADV_d)/2] = data16bit;
    udelay(10);

    /* enable HP Auto-MDIX on MII port 1 */
    data16bit = swreg16[(PORT1_MII_dp+AEGSTS_d)/2];
    data16bit &= ~0x0800;
    udelay(10);
    swreg16[(PORT1_MII_dp+AEGSTS_d)/2] = data16bit;
    udelay(10);

    // enable ISB abort timer
    transtimer = bridge->status;
    bridge->status = transtimer | 0x3FF;
    transtimer = bridge->status;
    /*
     * set MII port state override register for port 1 (external PHY)
     * set full duplex and 100Mbs (bit 2: 0 for 100Mbs, 1 for 10Mbs)
     */
    swreg16[(CTLREG_dp+MII_PSTS_d)/2] = 0xfffb;
}


/*
 * init_dma: Initialize DMA control register
 */
static void init_dma(bcm6352enet_softc *softc)
{
    volatile DmaChannel *txDma = softc->txDma;
    volatile DmaChannel *rxDma = softc->rxDma;

    // transmit
    txDma->cfg = 0;       /* initialize first (will enable later) */
    txDma->maxBurst = DMA_MAX_BURST_LENGTH;
    txDma->intMask = 0;   /* mask all ints */
    txDma->intStat = DMA_DONE|DMA_NO_DESC|DMA_BUFF_DONE;
    txDma->startAddr = (uint32_t) K1_TO_PHYS((uint32_t)softc->txBufPtr);

    // receive
    rxDma->cfg = 0;  // initialize first (will enable later)
    rxDma->maxBurst = DMA_MAX_BURST_LENGTH;
    rxDma->startAddr = (uint32_t)K1_TO_PHYS((uint32_t)softc->rxFirstBdPtr);
    rxDma->length = softc->nrRxBds;
    rxDma->fcThreshold = 0;
    rxDma->numAlloc = 0;
    rxDma->intMask = 0;   /* mask all ints */
    /* clr any pending interrupts on channel */
    rxDma->intStat = DMA_DONE|DMA_NO_DESC|DMA_BUFF_DONE;
    /* set to interrupt on packet complete and no descriptor available */
    rxDma->intMask = 0;
    /* configure DMA channels and enable Rx */
    rxDma->cfg = DMA_CHAINING|DMA_WRAP_EN;
}


/*
 * bcm6352_init_dev: initialize Ethernet Switch device,
 * allocate Tx/Rx buffer descriptors pool, Tx control block pool.
 */
static int bcm6352_init_dev(bcm6352enet_softc *softc)
{
    unsigned long i, j;
    unsigned char *p = NULL;

    /* make sure emac clock is on */
    INTC->blkEnables |= (ESWITCH_CLK_EN | EPHY_CLK_EN);

    /* setup buffer/pointer relationships here */
    softc->nrRxBds = NR_RX_BDS;
    softc->rxBufLen = ENET_MAX_MTU_SIZE;

    /* init rx/tx dma channels */
    softc->dmaChannels = (DmaChannel *)(DMA_BASE);
    softc->rxDma = &softc->dmaChannels[EMAC_RX_CHAN];
    softc->txDma = &softc->dmaChannels[EMAC_TX_CHAN];

    p = (unsigned char *) (((uint32_t) softc->txBuf + 0x10) & ~0x0f);
    softc->txBufPtr = (unsigned char *)K0_TO_K1((uint32_t) p);

    p = (unsigned char *) (((uint32_t) softc->rxMem + 0x10) & ~0x0f);
    softc->rxBds = (DmaDesc *)K0_TO_K1((uint32_t) p);
    p += softc->nrRxBds * sizeof(DmaDesc);
    softc->rxBuffers = (unsigned char *) K0_TO_PHYS((uint32_t) p);

    /* initialize rx ring pointer variables. */
    softc->rxBdAssignPtr = softc->rxBdReadPtr = softc->rxBds;
    softc->rxFirstBdPtr = softc->rxBds;
    softc->rxLastBdPtr = softc->rxFirstBdPtr + softc->nrRxBds - 1;

    /* init the receive buffer descriptor ring */
    for (i = 0, j = (unsigned long) softc->rxBuffers; i < softc->nrRxBds;
        i++, j += softc->rxBufLen)
    {
        (softc->rxFirstBdPtr + i)->length = softc->rxBufLen;
        (softc->rxFirstBdPtr + i)->address = j;
        (softc->rxFirstBdPtr + i)->status = DMA_OWN;
    }
    softc->rxLastBdPtr->status = DMA_OWN | DMA_WRAP;

    /* init dma registers */
    init_dma(softc);

    /* init switch control registers */
    init_mii(softc);

    softc->rxDma->cfg |= DMA_ENABLE;

    /* if we reach this point, we've init'ed successfully */
    return 0;
}


/*
 *  internal_open - initialize Ethernet device
 */
int internal_open( bcm6352enet_softc *softc )
{
    int status;
    int i;
    unsigned char macAddr[ETH_ALEN];
    int dualMacMode = 0;

    /* figure out which chip we're running on */
    softc->chipId  = (INTC->RevID & 0xFFFF0000) >> 16;
    softc->chipRev = (INTC->RevID & 0xFF);

    /* print the ChipID and module version info */
    printf("Broadcom BCM%X%X Ethernet Network Device ", 
        softc->chipId, softc->chipRev);
    printf(VER_STR "\n");

    clear_bit(0, &softc->dualMacMode);

    /* read NVRam setting on route mode or switch mode */
    dualMacMode = nvramData.ulEnetModeFlag;

    if (dualMacMode == 1)
        set_bit(0, &softc->dualMacMode);

    printf("bcm6352Enet configure as %s mode\n",
        softc->dualMacMode ? "MAC Isolation" : "Switching");

    if( (status = bcm6352_init_dev(softc)) == 0 )
    {
        /* Read first MAC address.  Only use first MAC even if dual MAC is
         * configured.
         */
        macAddr[0] = 0xff;
        memcpy(macAddr, nvramData.ucaBaseMacAddr, sizeof(macAddr));

        if( macAddr[0] == 0xff )
            memcpy( macAddr, "\x00\x10\x18\x00\x00\x01", 6 );

        /* fill in the MAC address */
        for (i = 0; i < 6; i++)
            softc->macAddr[i] = macAddr[i];

        printf( "MAC Address: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
            macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4],
            macAddr[5], macAddr[6], macAddr[7] );

        write_mac_address(softc);
    }

    return( status );
}


/*
 * write_mac_address: store MAC address into ARL table                    
 *                                                           
 * NOTE: THE MAC ADDRESS MUST BE NIBBLE-SWAPPED AND BIT-FLIPPED FOR THE 
 *       ARL TABLE
 */
void write_mac_address( bcm6352enet_softc *softc )
{
    unsigned char data8bit;
    int i;
    int j;

    if (test_bit(0, &softc->dualMacMode)) {
        unsigned char *addr;

        /* set to dual mac mode if in single mac mode*/
        if (!test_and_set_bit(1, &softc->dualMacMode)) {
            /* set secure source port mask */
            /* bit mask identifies port 0 and port 1 initiates a secure VPN session */
            swreg16[(ARLCTL_dp + SEC_SPMSK_d) /2] = 0x0003;
            /* set secure destination port mask */
            /* 
             *bit mask identifies SMP port which is designated as
             * the secure destination
             */
            swreg16[(ARLCTL_dp + SEC_DPMSK_d) /2] = 0x0400;

            /* disable unicast packets */
            /* 
             * if RX-UCST_EN bit enable, it will only receive unknown unicast.
             * To receive known unicast, it should set MIRROR_ENABLE and 
             * IN_MIRROR_FILTER, or MULTIPORT VECTOR/ADDRESS registers
             */
            data8bit = swreg8[(CTLREG_dp + SMP_CTL_d)];
            data8bit &= ~0x10; // disable RX_UCST_EN bit
            swreg8[(CTLREG_dp + SMP_CTL_d)] = data8bit;

            /* Enable Multiport Address */
            swreg8[(ARLCTL_dp + GARLCFG_d)] = 0x10;

            /* Set Multiport Vector 1 to port 10 */
            swreg16[(ARLCTL_dp + PORTVEC1_d)/2] = 0x0400;

            /* Set Multiport Vector 2 to port 10 */
            swreg16[(ARLCTL_dp + PORTVEC2_d)/2] = 0x0400;
        }

        /* Only set first MAC address */
        j = GRPADDR1_d;
        addr = softc->macAddr;
        swreg32[(ARLCTL_dp+j)/4] = 
            ((*(uint16_t *)(addr+2))<<16) | *(uint16_t *)(addr+4);
        swreg16[(ARLCTL_dp+j+4)/2] = *(uint16_t *)addr;
    }
    else {
        unsigned char tempAddress[ETH_ALEN];
        unsigned char flippedAddress[ETH_ALEN];

        memset(flippedAddress, 0x00, ETH_ALEN);
        /* set to single mac mode if in dual mac mode */
        if (test_and_clear_bit(1, &softc->dualMacMode)) {
            /* secure source port mask */
            swreg16[(ARLCTL_dp + SEC_SPMSK_d) /2] = 0x0000;
            /* secure destination port mask */
            swreg16[(ARLCTL_dp + SEC_DPMSK_d) /2] = 0x0000;

            /* enable unicast packets */
            data8bit = swreg8[(CTLREG_dp + SMP_CTL_d)];
            data8bit |= 0x10; // enable RX_UCST_EN bit
            swreg8[(CTLREG_dp + SMP_CTL_d)] = data8bit;

            /* disable multiport address */
            swreg8[(ARLCTL_dp + GARLCFG_d)] = 0x00;
        }

        memcpy(tempAddress, softc->macAddr, ETH_ALEN);
        /* nibble-swap and bit-flip */
        for(i=0; i<6; i++)
        {
            for(j=0; j<7; j++)
            {
                flippedAddress[i] |= tempAddress[i] & 0x01;
                flippedAddress[i] <<= 1;
                tempAddress[i] >>= 1;
            }
            flippedAddress[i] |= tempAddress[i] & 0x01;
        }
   
        /* storing to ARL table */   
        swreg32[(ARLACCS_dp+ARLA_ENTRY0_d)/4] = 
            ((*(uint16_t *)&flippedAddress[2])<<16) | *(uint16_t *)&flippedAddress[4];
        swreg32[(ARLACCS_dp+ARLA_ENTRY0_d+4)/4] = 
            0xc0000000 | *(uint16_t *)&flippedAddress[0] | (MANAGEMENT_PORT << 16);
        swreg32[(ARLACCS_dp+ARLA_MAC_d)/4] = 
            ((*(uint16_t *)&flippedAddress[2])<<16) | *(uint16_t *)&flippedAddress[4];
        swreg32[(ARLACCS_dp+ARLA_MAC_d+4)/4] = *(uint16_t *)&flippedAddress[0];
        swreg32[(ARLACCS_dp+ARLA_RWCTL_d)/4] = 0x80;
    }
}


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_open
 Purpose: CFE driver open entry point.
-------------------------------------------------------------------------- */
static int bcm6352_enet_open(cfe_devctx_t *ctx)
{
    bcm6352enet_softc *softc = (bcm6352enet_softc *) ctx->dev_softc;

    if ((softc->chipId == 0x6352) && (softc->chipRev == 0xa1))
        ext_mii_check();

    if (test_bit(0, &softc->dualMacMode))
        port_control(0, 1);

    return 0;
}


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_read
 Purpose: Returns a recevied data buffer.
-------------------------------------------------------------------------- */
static int bcm6352_enet_read( cfe_devctx_t *ctx, iocb_buffer_t *buffer )
{
    uint32_t rxEvents;
    unsigned char *dstptr;
    unsigned char *srcptr;
    volatile DmaDesc *CurrentBdPtr;
    bcm6352enet_softc *softc = (bcm6352enet_softc *) ctx->dev_softc;

    /* ============================= ASSERTIONS ============================= */

    if( ctx == NULL ) {
        xprintf( "No context\n" );
        return -1;
    }

    if( buffer == NULL ) {
        xprintf( "No dst buffer\n" );
        return -1;
    }

    if( buffer->buf_length != ENET_MAX_BUF_SIZE ) {
        xprintf( "dst buffer too small.\n" );
        xprintf( "actual size is %d\n", buffer->buf_length );
        return -1;
    }

    if( softc == NULL ) {
        xprintf( "softc has not been initialized.\n" );
        return -1;
    }

    /* ====================================================================== */

    dstptr       = buffer->buf_ptr;
    CurrentBdPtr = softc->rxBdReadPtr;

    if( (CurrentBdPtr->status & DMA_OWN) == 1 )
        return -1;

    srcptr = (unsigned char *)( PHYS_TO_K1(CurrentBdPtr->address) );
    memcpy( dstptr, srcptr, ETH_ALEN * 2 );
    dstptr += ETH_ALEN * 2;
    memcpy( dstptr, srcptr + HEDR_LEN, CurrentBdPtr->length - HEDR_LEN - 8 );

    /* length - header difference - 2 CRCs */
    buffer->buf_retlen = CurrentBdPtr->length - 6 - 8;

    CurrentBdPtr->length = ENET_MAX_MTU_SIZE;
    CurrentBdPtr->status &= DMA_WRAP;
    CurrentBdPtr->status |= DMA_OWN;

    IncRxBDptr(CurrentBdPtr, softc);
    softc->rxBdReadPtr = CurrentBdPtr;

    rxEvents = softc->rxDma->intStat;
    softc->rxDma->intStat = rxEvents;

    softc->rxDma->cfg = DMA_ENABLE | DMA_CHAINING | DMA_WRAP_EN;

    return 0;
}


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_inpstat
 Purpose: Indicates if data has been received.
-------------------------------------------------------------------------- */
static int bcm6352_enet_inpstat( cfe_devctx_t *ctx, iocb_inpstat_t *inpstat )
{
    bcm6352enet_softc *softc;
    volatile DmaDesc *CurrentBdPtr;

    /* ============================= ASSERTIONS ============================= */

    if( ctx == NULL ) {
        xprintf( "No context\n" );
        return -1;
    }

    if( inpstat == NULL ) {
        xprintf( "No inpstat buffer\n" );
        return -1;
    }

    softc = (bcm6352enet_softc *)ctx->dev_softc;
    if( softc == NULL ) {
        xprintf( "softc has not been initialized.\n" );
        return -1;
    }

    /* ====================================================================== */


    CurrentBdPtr = softc->rxBdReadPtr;

    /* inp_status == 1 -> data available */
    inpstat->inp_status = (CurrentBdPtr->status & DMA_OWN) ? 0 : 1;

    return 0;
}


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_write
 Purpose: Sends a data buffer.
-------------------------------------------------------------------------- */
static int bcm6352_enet_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    int copycount;
    unsigned int    srclen;
    unsigned char *dstptr;
    unsigned char *srcptr;
    bcm6352enet_softc *softc = (bcm6352enet_softc *) ctx->dev_softc;
    volatile DmaChannel *txDma = softc->txDma;

    /* ============================= ASSERTIONS ============================= */

    if( ctx == NULL ) {
        xprintf( "No context\n" );
        return -1;
    }

    if( buffer == NULL ) {
        xprintf( "No dst buffer\n" );
        return -1;
    }

    if( buffer->buf_length > ENET_MAX_BUF_SIZE ) {
        xprintf( "src buffer too large.\n" );
        xprintf( "size is %d\n", buffer->buf_length );
        return -1;
    }

    if( softc == NULL ) {
        xprintf( "softc has not been initialized.\n" );
        return -1;
    }

    /* ====================================================================== */


    /******** Convert header to Broadcom's special header format. ********/
    dstptr = softc->txBufPtr;
    srcptr = buffer->buf_ptr;
    srclen = buffer->buf_length;

    memcpy( dstptr, srcptr, ETH_ALEN * 2 );
    dstptr       += ETH_ALEN * 2;
    srcptr       += ETH_ALEN * 2;

    *((uint16_t *)dstptr) = BRCM_TYPE;
    dstptr += 2;

    if( srclen < 60 - 6 - 8 ) {
        *((uint16_t *)dstptr)  = (uint16_t)60;
    } else {
        *((uint16_t *)dstptr)  = (uint16_t)(srclen + 6 + 8);
    }
    dstptr += 2;

    *((uint16_t *)dstptr) = (uint16_t)MANAGEMENT_PORT;
    dstptr += 2;

    copycount = srclen - ETH_ALEN * 2;
    memcpy( dstptr, srcptr, copycount );

    if( srclen < 60 ) {
        dstptr += copycount;
        memset( dstptr, 0, 60 - srclen );
        txDma->length = 66;
    } else {
        txDma->length = srclen + 6;
    }

    /* Set status of DMA buffer to be transmitted. */
    txDma->bufStat = DMA_SOP | DMA_EOP | DMA_APPEND_CRC | DMA_OWN;

    /* Enable DMA for this channel. */
    softc->txDma->cfg |= DMA_ENABLE;

    /* poll the dma status until done. */
    while( (txDma->bufStat & DMA_OWN) == 0 )
        ;
    txDma->bufStat = 0;

    return 0;
}


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_ioctl
 Purpose: I/O Control function.
-------------------------------------------------------------------------- */
static int bcm6352_enet_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    bcm6352enet_softc *softc = (bcm6352enet_softc *) ctx->dev_softc;
    int retval = 0;

    switch( (int)buffer->buf_ioctlcmd ) {
        case IOCTL_ETHER_GETHWADDR:
            memcpy( buffer->buf_ptr, softc->macAddr, sizeof(softc->macAddr) );
            break;
        case IOCTL_ETHER_SETHWADDR:
            memcpy( softc->macAddr, buffer->buf_ptr, sizeof(softc->macAddr) );
            write_mac_address( softc );
            break;
        case IOCTL_ETHER_GETSPEED:
            xprintf( "BCM6352 : GETSPEED not implemented.\n" );
            retval = -1;
            break;
        case IOCTL_ETHER_SETSPEED:
            xprintf( "BCM6352 : SETSPEED not implemented.\n" );
            retval = -1;
            break;
        case IOCTL_ETHER_GETLINK:
            xprintf( "BCM6352 : GETLINK not implemented.\n" );
            retval = -1;
            break;
        case IOCTL_ETHER_GETLOOPBACK:
            xprintf( "BCM6352 : GETLOOPBACK not implemented.\n" );
            retval = -1;
            break;
        case IOCTL_ETHER_SETLOOPBACK:
            xprintf( "BCM6352 : SETLOOPBACK not implemented.\n" );
            retval = -1;
            break;
        default:
            xprintf( "Invalid IOCTL to bcm6352_enet_ioctl.\n" );
            retval = -1;
    }

    return retval;
}


/* --------------------------------------------------------------------------
    Name: bcm6352_enet_close
 Purpose: Uninitialize.
-------------------------------------------------------------------------- */
static int bcm6352_enet_close(cfe_devctx_t *ctx)
{
    bcm6352enet_softc *softc = (bcm6352enet_softc *) ctx->dev_softc;
    unsigned long sts;

    printf( "Closing Ethernet.\n" );

    if (test_bit(0, &softc->dualMacMode))
        port_control(0, 0);

    sts = softc->rxDma->intStat;
    softc->rxDma->intStat = sts;
    softc->rxDma->intMask = 0;
    softc->rxDma->cfg = 0;

    sts = softc->txDma->intStat;
    softc->txDma->intStat = sts;
    softc->txDma->intMask = 0;
    softc->txDma->cfg = 0;

    return 0;
}
