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
#ifndef _DEV_BCM6352ENET_H_
#define _DEV_BCM6352ENET_H_

#include "6352_map.h"

/*---------------------------------------------------------------------*/
/* specify number of BDs and buffers to use                            */
/*---------------------------------------------------------------------*/
#define NR_TX_BDS               100
#define NR_RX_BDS               100
#define ENET_MAX_BUF_SIZE       1514    /* Body(1500) + EH_SIZE(14) */
#define ENET_MAX_MTU_SIZE       1536    /* Body(1500) + EH_SIZE(14) + FCS(4) */
                                        /* + BRCM TYPE(2) + BRCM_TAG(4) +    */
                                        /* RECALCULATE CRC(4) + pad to 16    */
                                        /* boundary(8)                       */
#define HEDR_LEN                18
#define DMA_MAX_BURST_LENGTH    16      /* in 32 bit words */

#define ETH_ALEN                6

typedef struct Brcm_Hdr {
    unsigned char       dAddr[ETH_ALEN];/* destination hardware address */
    unsigned char       sAddr[ETH_ALEN];/* destination hardware address */
    uint16_t            brcmType;       /* special Broadcom type */
    uint32_t            brcmTag;        /* special Broadcom tag */
}__attribute__((packed)) Brcm_Hdr;

/*
 * device context
 */ 
typedef struct bcm6352enet_softc {
    unsigned long   dualMacMode;        /* dual mac mode */
    unsigned char   macAddr[ETH_ALEN];

    /* transmit variables */
    volatile DmaChannel *dmaChannels;
    volatile DmaChannel *txDma;         /* location of transmit DMA register set */
    unsigned char   txBuf[ENET_MAX_MTU_SIZE+16];
    unsigned char   *txBufPtr;

    /* receive variables */
    volatile DmaChannel *rxDma;         /* location of receive DMA register set */
    volatile DmaDesc *rxBds;            /* Memory location of rx bd ring */
    volatile DmaDesc *rxBdAssignPtr;    /* ptr to next rx bd to become full */
    volatile DmaDesc *rxBdReadPtr;      /* ptr to next rx bd to be processed */
    volatile DmaDesc *rxLastBdPtr;      /* ptr to last allocated rx bd */
    volatile DmaDesc *rxFirstBdPtr;     /* ptr to first allocated rx bd */
    int             nrRxBds;            /* number of receive bds */
    unsigned long   rxBufLen;           /* size of rx buffers for DMA */
    uint16_t        chipId;             /* chip's id */
    uint16_t        chipRev;            /* step */
    unsigned char   *rxBuffers;
    unsigned char   rxMem[NR_RX_BDS * (sizeof(DmaDesc)+ENET_MAX_MTU_SIZE+16)];
} bcm6352enet_softc;


// BD macros
#define IncRxBDptr(x, s) if (x == ((bcm6352enet_softc *)s)->rxLastBdPtr) \
                             x = ((bcm6352enet_softc *)s)->rxFirstBdPtr; \
                      else x++


/* The Broadcom type and tag fields */
#define BRCM_TYPE               0x8874

/* header length: DA (6) + SA (6) + BRCM_TYPE (2) + BRCM_TAG (4) */
#define HEADER_LENGTH 18             

/* SMP port bit definition */
#define SMP_PORT            0x40 /* frame management port */
#define SMP_PORT_ID         0x0A /* management port ID */
#define FORWARDING_STATE    0xA0 /* STP state */
#define RX_DISABLE          0x01 /* receive disable */
#define TX_DISABLE          0x02 /* transmit disable */
#define SW_FWDG_EN          0x02 /* software forwarding enabled */
#define MANAGED_MODE        0x01 /* managed mode */

/* PORT ID definition */
#define MANAGEMENT_PORT     10
#define PHY0_PORT           0
#define PHY1_PORT           1

static inline int test_bit(int nr, volatile void *addr)
{
    return ((1UL << (nr & 31)) & (((const unsigned int *) addr)[nr >> 5])) != 0;
}

static inline void set_bit(int nr, volatile void * addr)
{
    int mask;
    volatile int    *a = addr;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    *a |= mask;
}

static inline void clear_bit(int nr, volatile void * addr)
{
    int mask;
    volatile int    *a = addr;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    *a &= ~mask;
}

static inline int test_and_set_bit(int nr, volatile void * addr)
{
    int mask, retval;
    volatile int    *a = addr;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    retval = (mask & *a) != 0;
    *a |= mask;

    return retval;
}

static inline int test_and_clear_bit(int nr, volatile void * addr)
{
    int mask, retval;
    volatile int    *a = addr;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    retval = (mask & *a) != 0;
    *a &= ~mask;

    return retval;
}

#endif /* _DEV_BCM6352ENET_H_ */
