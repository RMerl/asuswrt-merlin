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
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_ioctl.h"
#include "cfe_device.h"
#include "cfe_devfuncs.h"
#include "sbmips.h"

#include "dev_bcm6345_eth.h"
extern int sysGetBoardId(void);

static void bcm6345_ether_probe( cfe_driver_t * drv,    unsigned long probe_a,
                                 unsigned long probe_b, void * probe_ptr );
static int bcm6345_ether_open(cfe_devctx_t *ctx);
static int bcm6345_ether_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm6345_ether_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm6345_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm6345_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm6345_ether_close(cfe_devctx_t *ctx);
static void bcm6345_write_mac_address(bcm6345_softc *softc, unsigned char *macAddr);

const static cfe_devdisp_t bcm6345_ether_dispatch = {
	bcm6345_ether_open,
	bcm6345_ether_read,
	bcm6345_ether_inpstat,
	bcm6345_ether_write,
	bcm6345_ether_ioctl,
	bcm6345_ether_close,
	NULL,
	NULL
};

const cfe_driver_t bcm6345_enet = {
	"BCM6345 Ethernet",
	"eth",
	CFE_DEV_NETWORK,
	&bcm6345_ether_dispatch,
	bcm6345_ether_probe
};

extern cfe_driver_t flashdrv;

static void bcm6345_ether_probe( cfe_driver_t * drv,    unsigned long probe_a,
                                 unsigned long probe_b, void * probe_ptr )
{
	bcm6345_softc * softc;
	char descr[100];

	softc = (bcm6345_softc *) KMALLOC( sizeof(bcm6345_softc), 0 );
	if( softc == NULL ) {
		xprintf( "BCM6345 : Failed to allocate softc memory.\n" );
	} else {
		memset( softc, 0, sizeof(bcm6345_softc) );

        if (internal_open( softc ) == -1) 
		    xprintf("Failed initializing enet hardware\n");
        else
        {
		    cfe_attach( drv, softc, NULL, descr );
            cfe_attach(&flashdrv, softc, NULL, NULL);
        }
	}

}


static int bcm6345_ether_open(cfe_devctx_t *ctx)
{
	return 0;
}



/*
 * mii_write: Write a value to the MII
 */
static void mii_write(uint32 uPhyAddr, uint32 uRegAddr, uint32 data)
{
    uint32 d;
    d = 0x50020000 | (uPhyAddr << 23) | (uRegAddr << 18) | data;
    EMAC->mdioData = d;
    delay_t(MII_WRITE_DELAY);
    while ( ! (EMAC->intStatus & 0x1) );
    EMAC->intStatus = 0x1;
}

/*
 * mii_read: Read a value from the MII
 */
static uint32 mii_read(uint32 uPhyAddr, uint32 uRegAddr) 
{
    EMAC->mdioData = 0x60020000 | (uPhyAddr << 23) | (uRegAddr << 18);
    delay_t(MII_WRITE_DELAY);
    while ( ! (EMAC->intStatus & 0x00000001) );
    EMAC->intStatus = 0x1;
    return EMAC->mdioData;
}

/* 
 * mii_GetConfig: Return the current MII configuration
 */
static MII_CONFIG mii_GetConfig(bcm6345_softc *softc)
{
	uint32 uData;
	MII_CONFIG eConfig = 0;

	/* Read the Link Partner Ability */
    uData = mii_read(softc->phyAddr, 0x05) & 0x0000FFFF;
    if (uData & 0x0100) {           /* 100 MB Full-Duplex */
        eConfig = (MII_100MBIT | MII_FULLDUPLEX);
    } else if (uData & 0x0080) {    /* 100 MB Half-Duplex */
        eConfig = MII_100MBIT;
    } else if (uData & 0x0040) {    /* 10 MB Full-Duplex */
        eConfig = MII_FULLDUPLEX;
    } 

	return eConfig;
}

/*
 * mii_AutoConfigure: Auto-Configure this MII interface
 */
static MII_CONFIG mii_AutoConfigure(bcm6345_softc *softc)
{
	int i;
	uint32 uData;
	MII_CONFIG eConfig;

    /* enable and restart autonegotiation */
	uData = mii_read(softc->phyAddr, 0) & 0x0000FFFF;
    uData |= (EPHY_RESTART_AUTONEGOTIATION | EPHY_AUTONEGOTIATE);
    mii_write(softc->phyAddr, 0x00, uData);

    /* wait for it to finish */
    for (i = 0; i < 1000; i++) {
	    delay_t(MII_READ_DELAY);
        uData = mii_read(softc->phyAddr, 0x01);
        if (uData & EPHY_AUTONEGOTIATION_COMPLETE) {
            break;
        }
    }

	eConfig = mii_GetConfig(softc);
	if (uData & EPHY_AUTONEGOTIATION_COMPLETE) {
		eConfig |= MII_AUTONEG;
	} 

    mii_write(softc->phyAddr, 0x1A, 0x0F00);

    return eConfig;
}

/*
 * soft_reset: Reset the MII
 */
static void soft_reset(uint32 uPhyAddr) 
{
    uint32 uData;

    mii_write(uPhyAddr, 0, 0x8000);
 
    delay_t(MII_WRITE_DELAY);    
    do {
        uData = mii_read(uPhyAddr, 0);
    } while (uData & 0x00008000);
}
/*
 * init_emac: Initializes the Ethernet control registers
 */
static void init_emac(bcm6345_softc *softc)
{
    MII_CONFIG eMiiConfig;

    /* disable ethernet MAC while updating its registers */
    EMAC->config = EMAC_DISABLE ;           
    while(EMAC->config & EMAC_DISABLE);     

    /* issue soft reset, wait for it to complete */
    EMAC->config = EMAC_SOFT_RESET;
    while (EMAC->config & EMAC_SOFT_RESET);

    switch(sysGetBoardId())
    {
    case BOARD_ID_BCM96345GW:
    case BOARD_ID_BCM96345I:
        softc->phyAddr = MII_EXTERNAL;
        break;

    case BOARD_ID_BCM96345R00:
    case BOARD_ID_BCM96345R10:
    case BOARD_ID_BCM96345SV:
    case BOARD_ID_BCM96345USB:
    default:
        softc->phyAddr = MII_INTERNAL;
        break;
    }

    /* init mii clock, do soft reset of phy, default is 10Base-T */
    if (softc->phyAddr == MII_EXTERNAL) {
        EMAC->config = EMAC_EXT_PHY;
        EMAC->mdioFreq = EMAC_MII_PRE_EN | 0x005;
        EMAC->txControl = EMAC_FD;  /* FULL DUPLEX */
    } else {
        /* init mii clock, do soft reset of phy, default is 10Base-T */
        EMAC->mdioFreq = EMAC_MII_PRE_EN | 0x01F;
        soft_reset(softc->phyAddr);

        eMiiConfig = mii_AutoConfigure(softc);

        if (! (eMiiConfig & MII_AUTONEG)) {
	        xprintf("Auto-negotiation timed-out\n");
        }

        if (eMiiConfig & (MII_100MBIT | MII_FULLDUPLEX)) {
	        xprintf("100 MB Full-Duplex (auto-neg)\n");
        } else if (eMiiConfig & MII_100MBIT) {
	        xprintf("100 MB Half-Duplex (auto-neg)\n");
        } else if (eMiiConfig & MII_FULLDUPLEX) {
	        xprintf("0 MB Full-Duplex (auto-neg)\n");
        } else {
	        xprintf("0 MB Half-Duplex (assumed)\n");
        }
        
        /* Check for FULL/HALF duplex */
        if (eMiiConfig & MII_FULLDUPLEX) {
            EMAC->txControl = EMAC_FD;  /* FULL DUPLEX */
        }
    }

    /* Initialize emac registers */
    EMAC->rxControl = EMAC_FC_EN | EMAC_PROM;   // allow Promiscuous

#ifdef MAC_LOOPBACK
    EMAC->rxControl |= EMAC_LOOPBACK;
#endif
    EMAC->rxMaxLength = ENET_MAX_MTU_SIZE;
    EMAC->txMaxLength = ENET_MAX_MTU_SIZE;

    /* tx threshold = abs(63-(0.63*EMAC_DMA_MAX_BURST_LENGTH)) */
    EMAC->txThreshold = 16;
    EMAC->mibControl  = 0x01;       /* clear MIBs on read */
    EMAC->intMask = 0;              /* mask all EMAC interrupts*/
    
    EMAC->config |= EMAC_ENABLE;    
}
/*
 * init_dma: Initialize DMA control register
 */
static void init_dma(bcm6345_softc *softc)
{
    // transmit
    softc->txDma->cfg = 0;       /* initialize first (will enable later) */
    softc->txDma->maxBurst = DMA_MAX_BURST_LENGTH;
    softc->txDma->startAddr = K1_TO_PHYS((uint32_t)(softc->txBds));
    softc->txDma->length = NR_TX_BDS;
    softc->txDma->fcThreshold = 0;
    softc->txDma->numAlloc = 0;
    softc->txDma->intMask = 0;   /* mask all ints */
    /* clr any pending interrupts on channel */
    softc->txDma->intStat = DMA_DONE|DMA_NO_DESC|DMA_BUFF_DONE;
    softc->txDma->intMask = DMA_DONE;

    // receive
    softc->rxDma->cfg = 0;  // initialize first (will enable later)
    softc->rxDma->maxBurst = DMA_MAX_BURST_LENGTH;
    softc->rxDma->startAddr = K1_TO_PHYS((uint32_t)softc->rxBds);
    softc->rxDma->length = NR_RX_BDS;
    softc->rxDma->fcThreshold = 0;
    softc->rxDma->numAlloc = 0;
    softc->rxDma->intMask = 0;   /* mask all ints */
    /* clr any pending interrupts on channel */
    softc->rxDma->intStat = DMA_DONE|DMA_NO_DESC|DMA_BUFF_DONE;
    softc->rxDma->intMask = DMA_DONE;

    /* configure DMA channels and enable Rx */
    softc->txDma->cfg = DMA_CHAINING|DMA_WRAP_EN|DMA_FLOWC_EN;
    softc->rxDma->cfg = DMA_CHAINING|DMA_WRAP_EN|DMA_FLOWC_EN;
}


static int internal_open(bcm6345_softc * softc)
{

    int i;
    void *p;

    /* make sure emac clock is on */
    INTC->blkEnables |= EMAC_CLK_EN;

    /* init rx/tx dma channels */
    softc->dmaChannels = (DmaChannel *)(DMA_BASE);
    softc->rxDma = &softc->dmaChannels[EMAC_RX_CHAN];
    softc->txDma = &softc->dmaChannels[EMAC_TX_CHAN];

	p = KMALLOC( NR_TX_BDS * sizeof(DmaDesc), 0 );
	if( p == NULL ) {
		xprintf( "BCM6345 : Failed to allocate txBds memory.\n" );
		KFREE( (void *)(softc->rxBds) );
		softc->rxBds = NULL;
		return -1;
	}

    softc->txBds = (DmaDesc *)K0_TO_K1((uint32) p);
	p = KMALLOC( NR_RX_BDS * sizeof(DmaDesc), 0 );
	if( p== NULL ) {
		xprintf( "BCM6345 : Failed to allocate rxBds memory.\n" );
		return -1;
	}
    softc->rxBds = (DmaDesc *)K0_TO_K1((uint32) p);


	softc->rxBuffers = (uint32_t)KMALLOC( NR_RX_BDS * ENET_MAX_MTU_SIZE, 0 );
	if( softc->rxBuffers == NULL ) {
		xprintf( "BCM6345 : Failed to allocate RxBuffer memory.\n" );
		KFREE( (void *)(softc->txBds) );
		softc->txBds = NULL;
		KFREE( (void *)(softc->rxBds) );
		softc->rxBds = NULL;
		return -1;
	}

	softc->txBuffers = (uint32_t)KMALLOC( NR_TX_BDS * ENET_MAX_MTU_SIZE, 0 );
	if( softc->txBuffers == NULL ) {
		xprintf( "BCM6345 : Failed to allocate txBuffer memory.\n" );
		KFREE( (void *)(softc->rxBuffers) );
		softc->rxBuffers = NULL;
		KFREE( (void *)(softc->txBds) );
		softc->txBds     = NULL;
		KFREE( (void *)(softc->rxBds) );
		softc->rxBds     = NULL;
		return -1;
	}

    /* Init the Receive Buffer Descriptor Ring. */
	softc->rxFirstBdPtr = softc->rxBdReadPtr = softc->rxBds;     
	softc->rxLastBdPtr = softc->rxBds + NR_RX_BDS - 1;

	for(i = 0; i < NR_RX_BDS; i++) {
		(softc->rxBds + i)->status  = DMA_OWN;
		(softc->rxBds + i)->length  = ENET_MAX_MTU_SIZE;
		(softc->rxBds + i)->address = softc->rxBuffers + i * ENET_MAX_MTU_SIZE;
		(softc->rxBds + i)->address = K1_TO_PHYS( (softc->rxBds + i)->address );
        softc->rxDma->numAlloc = 1;
	}
	softc->rxLastBdPtr->status |= DMA_WRAP;

	/* Init Transmit Buffer Descriptor Ring. */
	softc->txFirstBdPtr = softc->txNextBdPtr =  softc->txBds;
	softc->txLastBdPtr = softc->txBds + NR_TX_BDS - 1;

	for(i = 0; i < NR_TX_BDS; i++) {
		(softc->txBds + i)->status  = 0;
		(softc->txBds + i)->length  = 0;  //ENET_MAX_MTU_SIZE;
		(softc->txBds + i)->address = softc->txBuffers + i * ENET_MAX_MTU_SIZE;
		(softc->txBds + i)->address = K1_TO_PHYS( (softc->txBds + i)->address );
	}
	softc->txLastBdPtr->status = DMA_WRAP;

    /* init dma registers */
    init_dma(softc);

    /* init enet control registers */
    init_emac(softc);
    memcpy(softc->hwaddr, "\x18\x10\x18\x01\x00\x01", ETH_ALEN);

    bcm6345_write_mac_address( softc, softc->hwaddr);
    
    softc->rxDma->cfg |= DMA_ENABLE;

    return 0;
}



static int bcm6345_ether_read( cfe_devctx_t * ctx, iocb_buffer_t * buffer )
{
	uint32_t        rxEvents;
	unsigned char * dstptr;
	unsigned char * srcptr;
	volatile DmaDesc * CurrentBdPtr;
	bcm6345_softc * softc = (bcm6345_softc *) ctx->dev_softc;
    uint16 dmaFlag;

	if( ctx == NULL ) {
		xprintf( "No context\n" );
		return -1;
	}

	if( buffer == NULL ) {
		xprintf( "No dst buffer\n" );
		return -1;
	}

	if( buffer->buf_length > ENET_MAX_MTU_SIZE ) {
		xprintf( "dst buffer too small.\n" );
		xprintf( "actual size is %d\n", buffer->buf_length );
		return -1;
	}

	if( softc == NULL ) {
		xprintf( "softc has not been initialized.\n" );
		return -1;
	}

    dmaFlag = (uint16) softc->rxBdReadPtr->status;
    if (!(dmaFlag & DMA_EOP))
    {
        xprintf("dmaFlag (return -1)[%04x]\n", dmaFlag);
        return -1;
    }

	dstptr       = buffer->buf_ptr;
	CurrentBdPtr = softc->rxBdReadPtr;

	srcptr = (unsigned char *)( PHYS_TO_K1(CurrentBdPtr->address) );

    buffer->buf_retlen = CurrentBdPtr->length;
	memcpy( dstptr, srcptr, buffer->buf_retlen );

	CurrentBdPtr->length = ENET_MAX_MTU_SIZE;
	CurrentBdPtr->status = DMA_OWN;
	if( CurrentBdPtr == softc->rxLastBdPtr ) 
		CurrentBdPtr->status |= DMA_WRAP;

    IncRxBdPtr( CurrentBdPtr, softc );
	softc->rxBdReadPtr = CurrentBdPtr;
    softc->rxDma->numAlloc = 1;

    rxEvents = softc->rxDma->intStat;

	if (rxEvents & DMA_BUFF_DONE)
    {
		softc->rxDma->intStat = DMA_BUFF_DONE;	 // clr interrupt
	}
    //Complete packet placed in memory.
    if (rxEvents & DMA_DONE)
    {
        softc->rxDma->intStat = DMA_DONE;  // clr interrupt
    }
    else {
		return -1;
	}

	if (rxEvents & DMA_NO_DESC)
    {
		softc->rxDma->intStat |= DMA_NO_DESC;	 // clr interrupt
	}

    // enable rx dma
    softc->rxDma->cfg = DMA_ENABLE |DMA_CHAINING|DMA_WRAP_EN;
	return 0;
}


static int bcm6345_ether_inpstat( cfe_devctx_t * ctx, iocb_inpstat_t * inpstat )
{
	bcm6345_softc    * softc;
	volatile DmaDesc * CurrentBdPtr;

	/* ============================= ASSERTIONS ============================= */

	if( ctx == NULL ) {
		xprintf( "No context\n" );
		return -1;
	}

	if( inpstat == NULL ) {
		xprintf( "No inpstat buffer\n" );
		return -1;
	}

	softc = (bcm6345_softc *)ctx->dev_softc;
	if( softc == NULL ) {
		xprintf( "softc has not been initialized.\n" );
		return -1;
	}

	/* ====================================================================== */


	CurrentBdPtr = softc->rxBdReadPtr;

    // inp_status == 1 -> data available
	inpstat->inp_status = (CurrentBdPtr->status & DMA_OWN) ? 0 : 1;

	return 0;
}


static int bcm6345_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	uint32_t status;
	unsigned char * dstptr;
	bcm6345_softc * softc;
	volatile DmaDesc * CurrentBdPtr;
	volatile uint32 txEvents = 0;

	/* ============================= ASSERTIONS ============================= */

	if( ctx == NULL ) {
		xprintf( "No context\n" );
		return -1;
	}

	if( buffer == NULL ) {
		xprintf( "No dst buffer\n" );
		return -1;
	}

	if( buffer->buf_length > ENET_MAX_MTU_SIZE ) {
		xprintf( "src buffer too large.\n" );
		xprintf( "size is %d\n", buffer->buf_length );
		return -1;
	}

	softc = (bcm6345_softc *) ctx->dev_softc;
	if( softc == NULL ) {
		xprintf( "softc has not been initialized.\n" );
		return -1;
	}

	/* ====================================================================== */

	CurrentBdPtr = softc->txNextBdPtr;

	/* Find out if the next BD is available. */
	if( CurrentBdPtr->status & DMA_OWN ) {
		xprintf( "No tx BD available ?!\n" );
		return -1;
	}

	dstptr = (unsigned char *)PHYS_TO_K1( CurrentBdPtr->address );
	memcpy( dstptr, buffer->buf_ptr, buffer->buf_length );

	/* Set status of DMA BD to be transmitted. */
	status = DMA_SOP | DMA_EOP | DMA_APPEND_CRC | DMA_OWN;
	if( CurrentBdPtr == softc->txLastBdPtr ) {
		status |= DMA_WRAP;
	}
    CurrentBdPtr->length = buffer->buf_length;
	CurrentBdPtr->status = status;

    // Enable DMA for this channel
    softc->txDma->cfg |= DMA_ENABLE;

    // poll the dma status until done
    do
    {
        txEvents = CurrentBdPtr->status; 
    } while (txEvents & DMA_OWN);


    //Advance BD pointer to next in the chain.
	InctxBdPtr( CurrentBdPtr, softc );
	softc->txNextBdPtr = CurrentBdPtr;

	return 0;
}

/*
 * perfectmatch_write: write an address to the EMAC perfect match registers
 */
static void perfectmatch_write(int reg, unsigned char *pAddr, bool bValid)
{
    volatile uint32 *pmDataLo;
    volatile uint32 *pmDataHi;
  
    switch (reg) {
    case 0:
        pmDataLo = &EMAC->pm0DataLo;
        pmDataHi = &EMAC->pm0DataHi;
        break;
    case 1:
        pmDataLo = &EMAC->pm1DataLo;
        pmDataHi = &EMAC->pm1DataHi;
        break;
    case 2:
        pmDataLo = &EMAC->pm2DataLo;
        pmDataHi = &EMAC->pm0DataHi;
        break;
    case 3:
        pmDataLo = &EMAC->pm3DataLo;
        pmDataHi = &EMAC->pm3DataHi;
        break;
    default:
        return;
    }
    /* Fill DataHi/Lo */
    if (bValid == 1)
        *pmDataLo = MAKE4((pAddr + 2));
    *pmDataHi = MAKE2(pAddr) | (bValid << 16);
}


/*
 * bcm6345_write_mac_address: store MAC address into EMAC perfect match registers                   
 */
static void bcm6345_write_mac_address(bcm6345_softc *softc, unsigned char *macAddr)
{
    volatile uint32 data32bit;

    data32bit = EMAC->config;
    if (data32bit & EMAC_ENABLE) {
        /* disable ethernet MAC while updating its registers */
        EMAC->config &= ~EMAC_ENABLE ;           
        EMAC->config |= EMAC_DISABLE ;           
        while(EMAC->config & EMAC_DISABLE);     
    }
    
    perfectmatch_write(0, macAddr, 1);

    if (data32bit & EMAC_ENABLE) {
        EMAC->config = data32bit;
    }
}


static int bcm6345_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
	int retval = 0;

    return retval;
}


static int bcm6345_ether_close(cfe_devctx_t *ctx)
{
    bcm6345_softc * softc = (bcm6345_softc *) ctx->dev_softc;
    unsigned long sts;
    
    sts = softc->rxDma->intStat;
    softc->rxDma->intStat = sts;
    softc->rxDma->intMask = 0;
    softc->rxDma->cfg = 0;

    sts = softc->txDma->intStat;
    softc->txDma->intStat = sts;
    softc->txDma->intMask = 0;
    softc->txDma->cfg = 0;

    /* return buffer allocation register internal count to 0 */
    softc->rxDma->numAlloc = (NR_RX_BDS * -1);;

    EMAC->config = EMAC_DISABLE;
    KFREE( (void *)(softc->txBuffers) );
    KFREE( (void *)(softc->rxBuffers) );
    KFREE( (void *)(softc->txBds) );
    KFREE( (void *)(softc->rxBds) );
    return 0;
}

static void delay_t(int ticks)
{
    int32_t t;
    int tenTicks = ticks * 15;      //~~~

    t = _getticks() + tenTicks;
    while (_getticks() < t) ; /* NULL LOOP */
}
