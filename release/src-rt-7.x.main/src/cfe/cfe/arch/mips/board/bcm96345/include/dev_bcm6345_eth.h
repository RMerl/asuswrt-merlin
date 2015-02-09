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
#ifndef __BCM6345_ETH_H
#define __BCM6345_ETH_H

#include "board.h"
#include "6345_map.h"
#include "6345_common.h"

#define MII_READ_DELAY  1000
#define MII_WRITE_DELAY 10000

// from linux if_ether.h
#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#define ETH_HLEN	14		/* Total octets in header.	 */
#define ETH_ZLEN	60		/* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500		/* Max. octets in payload	 */
// end if_ether.h

/*---------------------------------------------------------------------*/
/* specify number of BDs and buffers to use                            */
/*---------------------------------------------------------------------*/
#define NR_TX_BDS               3
#define NR_RX_BDS               3
#define ENET_MAX_MTU_SIZE       1520    /* Body(1500) + EH_SIZE(14) + FCS(4) */
#define DMA_MAX_BURST_LENGTH    16      /* in 32 bit words */

#define BRCM_TYPE 0x8874

/* MII Control */
#define EPHY_LOOPBACK                   0x4000
#define EPHY_AUTONEGOTIATE              0x1000
#define EPHY_100MBIT                    0x2000
#define EPHY_FULLDUPLEX                 0x0100
#define EPHY_RESTART_AUTONEGOTIATION    0x0200
/* MII STATUS */
#define EPHY_AUTONEGOTIATION_COMPLETE   0x0020

typedef enum {
    MII_EXTERNAL = 0,
    MII_INTERNAL = 1,
}   MII_PHYADDR;
#define EPHY_ADDR           MII_INTERNAL

typedef enum {
    MII_100MBIT     = 0x0001,
    MII_FULLDUPLEX  = 0x0002,
    MII_AUTONEG     = 0x0004,
}   MII_CONFIG;

#define MAKE4(x)   ((x[3] & 0xFF) | ((x[2] & 0xFF) << 8) | ((x[1] & 0xFF) << 16) | ((x[0] & 0xFF) << 24))
#define MAKE2(x)   ((x[1] & 0xFF) | ((x[0] & 0xFF) << 8))


#define ERROR(x)        xsprintf x
#ifndef ASSERT
#define ASSERT(x)       if (x); else ERROR(("assert: "__FILE__" line %d\n", __LINE__)); 
#endif

//#define DUMP_TRACE

#if defined(DUMP_TRACE)
#define TRACE (x)         xprintf x
#else
#define TRACE(x)
#endif

typedef struct PM_Addr {
    uint16              valid;          /* 1 indicates the corresponding address is valid */
    unsigned char       dAddr[ETH_ALEN];/* perfect match register's destination address */
    unsigned int        ref;            /* reference count */
} PM_Addr;					 
#define MAX_PMADDR      4               /* # of perfect match address */

typedef struct bcm6345_softc {

    volatile DmaChannel *dmaChannels;
    
    /* transmit variables */    
    volatile DmaChannel *txDma;         /* location of transmit DMA register set */
    volatile DmaDesc *txBds;            /* Memory location of tx Dma BD ring */
    volatile DmaDesc *txFirstBdPtr;     /* ptr to first allocated Tx BD */
    volatile DmaDesc *txNextBdPtr;      /* ptr to next Tx BD to transmit with */
    volatile DmaDesc *txLastBdPtr;      /* ptr to last allocated Tx BD */

    /* receive variables */
    volatile DmaChannel *rxDma;         /* location of receive DMA register set */
    volatile DmaDesc *rxBds;            /* Memory location of rx bd ring */
    volatile DmaDesc *rxFirstBdPtr;     /* ptr to first allocated rx bd */
    volatile DmaDesc *rxBdReadPtr;      /* ptr to next rx bd to be processed */
    volatile DmaDesc *rxLastBdPtr;      /* ptr to last allocated rx bd */

	uint32_t rxBuffers;
	uint32_t txBuffers;

    uint16          chipId;             /* chip's id */
    uint16          chipRev;            /* step */
    int             phyAddr;            /* 1 - external MII, 0 - internal MII (default after reset) */
//    PM_Addr         pmAddr[MAX_PMADDR]; /* perfect match address */
	uint8_t         hwaddr[ETH_ALEN];

} bcm6345_softc;



#define IncRxBdPtr(x, s) if (x == ((bcm6345_softc *)s)->rxLastBdPtr) \
                             x = ((bcm6345_softc *)s)->rxBds; \
                      else x++
#define InctxBdPtr(x, s) if (x == ((bcm6345_softc *)s)->txLastBdPtr) \
                             x = ((bcm6345_softc *)s)->txBds; \
                      else x++

// extern and function prototype

extern int32_t _getticks(void);
static void delay_t(int ticks);
static int internal_open(bcm6345_softc * softc);

#ifdef DUMP_DATA
static void hexdump( unsigned char * src, int srclen, int rowlen, int rows );
#endif

#endif // __BCM6345_ETH_H
