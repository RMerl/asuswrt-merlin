/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * File: desc.h
 *
 * Purpose:The header file of descriptor
 *
 * Revision History:
 *
 * Author: Tevin Chen
 *
 * Date: May 21, 1996
 *
 */

#ifndef __DESC_H__
#define __DESC_H__

#include <linux/types.h>
#include <linux/mm.h>
#include "ttype.h"
#include "tether.h"

/*---------------------  Export Definitions -------------------------*/

#define B_OWNED_BY_CHIP     1           //
#define B_OWNED_BY_HOST     0           //

//
// Bits in the RSR register
//
#define RSR_ADDRBROAD       0x80        // 1000 0000
#define RSR_ADDRMULTI       0x40        // 0100 0000
#define RSR_ADDRUNI         0x00        // 0000 0000
#define RSR_IVLDTYP         0x20        // 0010 0000 , invalid packet type
#define RSR_IVLDLEN         0x10        // 0001 0000 , invalid len (> 2312 byte)
#define RSR_BSSIDOK         0x08        // 0000 1000
#define RSR_CRCOK           0x04        // 0000 0100
#define RSR_BCNSSIDOK       0x02        // 0000 0010
#define RSR_ADDROK          0x01        // 0000 0001

//
// Bits in the new RSR register
//
#define NEWRSR_DECRYPTOK    0x10        // 0001 0000
#define NEWRSR_CFPIND       0x08        // 0000 1000
#define NEWRSR_HWUTSF       0x04        // 0000 0100
#define NEWRSR_BCNHITAID    0x02        // 0000 0010
#define NEWRSR_BCNHITAID0   0x01        // 0000 0001

//
// Bits in the TSR0 register
//
#define TSR0_PWRSTS1_2      0xC0        // 1100 0000
#define TSR0_PWRSTS7        0x20        // 0010 0000
#define TSR0_NCR            0x1F        // 0001 1111

//
// Bits in the TSR1 register
//
#define TSR1_TERR           0x80        // 1000 0000
#define TSR1_PWRSTS4_6      0x70        // 0111 0000
#define TSR1_RETRYTMO       0x08        // 0000 1000
#define TSR1_TMO            0x04        // 0000 0100
#define TSR1_PWRSTS3        0x02        // 0000 0010
#define ACK_DATA            0x01        // 0000 0000

//
// Bits in the TCR register
//
#define EDMSDU              0x04        // 0000 0100 end of sdu
#define TCR_EDP             0x02        // 0000 0010 end of packet
#define TCR_STP             0x01        // 0000 0001 start of packet

// max transmit or receive buffer size
#define CB_MAX_BUF_SIZE     2900U       // max buffer size
                                        // NOTE: must be multiple of 4
#define CB_MAX_TX_BUF_SIZE          CB_MAX_BUF_SIZE // max Tx buffer size
#define CB_MAX_RX_BUF_SIZE_NORMAL   CB_MAX_BUF_SIZE // max Rx buffer size when not use Multi-RD

#define CB_BEACON_BUF_SIZE  512U        // default beacon buffer size

#define CB_MAX_RX_DESC      128         // max # of descriptor
#define CB_MIN_RX_DESC      16          // min # of rx descriptor
#define CB_MAX_TX_DESC      64          // max # of descriptor
#define CB_MIN_TX_DESC      16          // min # of tx descriptor

#define CB_MAX_RECEIVED_PACKETS     16  // max # of received packets at one time
                                        // limit our receive routine to indicating
                                        // this many at a time for 2 reasons:
                                        // 1. driver flow control to protocol layer
                                        // 2. limit the time used in ISR routine

#define CB_EXTRA_RD_NUM     32          // default # of Extra RD
#define CB_RD_NUM           32          // default # of RD
#define CB_TD_NUM           32          // default # of TD


// max number of physical segments
// in a single NDIS packet. Above this threshold, the packet
// is copied into a single physically contiguous buffer
#define CB_MAX_SEGMENT      4

#define CB_MIN_MAP_REG_NUM  4
#define CB_MAX_MAP_REG_NUM  CB_MAX_TX_DESC

#define CB_PROTOCOL_RESERVED_SECTION    16



// if retrys excess 15 times , tx will abort, and
// if tx fifo underflow, tx will fail
// we should try to resend it
#define CB_MAX_TX_ABORT_RETRY   3

#ifdef __BIG_ENDIAN

// WMAC definition FIFO Control
#define FIFOCTL_AUTO_FB_1   0x0010 // 0001 0000 0000 0000
#define FIFOCTL_AUTO_FB_0   0x0008 // 0000 1000 0000 0000
#define FIFOCTL_GRPACK      0x0004 // 0000 0100 0000 0000
#define FIFOCTL_11GA        0x0003 // 0000 0011 0000 0000
#define FIFOCTL_11GB        0x0002 // 0000 0010 0000 0000
#define FIFOCTL_11B         0x0001 // 0000 0001 0000 0000
#define FIFOCTL_11A         0x0000 // 0000 0000 0000 0000
#define FIFOCTL_RTS         0x8000 // 0000 0000 1000 0000
#define FIFOCTL_ISDMA0      0x4000 // 0000 0000 0100 0000
#define FIFOCTL_GENINT      0x2000 // 0000 0000 0010 0000
#define FIFOCTL_TMOEN       0x1000 // 0000 0000 0001 0000
#define FIFOCTL_LRETRY      0x0800 // 0000 0000 0000 1000
#define FIFOCTL_CRCDIS      0x0400 // 0000 0000 0000 0100
#define FIFOCTL_NEEDACK     0x0200 // 0000 0000 0000 0010
#define FIFOCTL_LHEAD       0x0100 // 0000 0000 0000 0001

//WMAC definition Frag Control
#define FRAGCTL_AES         0x0003 // 0000 0011 0000 0000
#define FRAGCTL_TKIP        0x0002 // 0000 0010 0000 0000
#define FRAGCTL_LEGACY      0x0001 // 0000 0001 0000 0000
#define FRAGCTL_NONENCRYPT  0x0000 // 0000 0000 0000 0000
//#define FRAGCTL_AC3         0x0C00 // 0000 0000 0000 1100
//#define FRAGCTL_AC2         0x0800 // 0000 0000 0000 1000
//#define FRAGCTL_AC1         0x0400 // 0000 0000 0000 0100
//#define FRAGCTL_AC0         0x0000 // 0000 0000 0000 0000
#define FRAGCTL_ENDFRAG     0x0300 // 0000 0000 0000 0011
#define FRAGCTL_MIDFRAG     0x0200 // 0000 0000 0000 0010
#define FRAGCTL_STAFRAG     0x0100 // 0000 0000 0000 0001
#define FRAGCTL_NONFRAG     0x0000 // 0000 0000 0000 0000

#else

#define FIFOCTL_AUTO_FB_1   0x1000 // 0001 0000 0000 0000
#define FIFOCTL_AUTO_FB_0   0x0800 // 0000 1000 0000 0000
#define FIFOCTL_GRPACK      0x0400 // 0000 0100 0000 0000
#define FIFOCTL_11GA        0x0300 // 0000 0011 0000 0000
#define FIFOCTL_11GB        0x0200 // 0000 0010 0000 0000
#define FIFOCTL_11B         0x0100 // 0000 0001 0000 0000
#define FIFOCTL_11A         0x0000 // 0000 0000 0000 0000
#define FIFOCTL_RTS         0x0080 // 0000 0000 1000 0000
#define FIFOCTL_ISDMA0      0x0040 // 0000 0000 0100 0000
#define FIFOCTL_GENINT      0x0020 // 0000 0000 0010 0000
#define FIFOCTL_TMOEN       0x0010 // 0000 0000 0001 0000
#define FIFOCTL_LRETRY      0x0008 // 0000 0000 0000 1000
#define FIFOCTL_CRCDIS      0x0004 // 0000 0000 0000 0100
#define FIFOCTL_NEEDACK     0x0002 // 0000 0000 0000 0010
#define FIFOCTL_LHEAD       0x0001 // 0000 0000 0000 0001

//WMAC definition Frag Control
#define FRAGCTL_AES         0x0300 // 0000 0011 0000 0000
#define FRAGCTL_TKIP        0x0200 // 0000 0010 0000 0000
#define FRAGCTL_LEGACY      0x0100 // 0000 0001 0000 0000
#define FRAGCTL_NONENCRYPT  0x0000 // 0000 0000 0000 0000
//#define FRAGCTL_AC3         0x000C // 0000 0000 0000 1100
//#define FRAGCTL_AC2         0x0008 // 0000 0000 0000 1000
//#define FRAGCTL_AC1         0x0004 // 0000 0000 0000 0100
//#define FRAGCTL_AC0         0x0000 // 0000 0000 0000 0000
#define FRAGCTL_ENDFRAG     0x0003 // 0000 0000 0000 0011
#define FRAGCTL_MIDFRAG     0x0002 // 0000 0000 0000 0010
#define FRAGCTL_STAFRAG     0x0001 // 0000 0000 0000 0001
#define FRAGCTL_NONFRAG     0x0000 // 0000 0000 0000 0000

#endif // #ifdef __BIG_ENDIAN

//#define TYPE_AC0DMA     0
//#define TYPE_TXDMA0     1
#define TYPE_TXDMA0     0
#define TYPE_AC0DMA     1
#define TYPE_ATIMDMA    2
#define TYPE_SYNCDMA    3
#define TYPE_MAXTD      2

#define TYPE_BEACONDMA  4

#define TYPE_RXDMA0     0
#define TYPE_RXDMA1     1
#define TYPE_MAXRD      2



// TD_INFO flags control bit
#define TD_FLAGS_NETIF_SKB               0x01       // check if need release skb
#define TD_FLAGS_PRIV_SKB                0x02       // check if called from private skb(hostap)
#define TD_FLAGS_PS_RETRY                0x04       // check if PS STA frame re-transmit
//#define TD_FLAGS_NETIF_SKB                0x04

/*---------------------  Export Types  ------------------------------*/

// ref_sk_buff is used for mapping the skb structure between pre-built driver-obj & running kernel.
// Since different kernel version (2.4x) may change skb structure, i.e. pre-built driver-obj
// may link to older skb that leads error.

typedef struct tagDEVICE_RD_INFO {
    struct sk_buff* skb;
    dma_addr_t  skb_dma;
    dma_addr_t  curr_desc;
} DEVICE_RD_INFO,   *PDEVICE_RD_INFO;

/*
static inline PDEVICE_RD_INFO alloc_rd_info(void) {
    PDEVICE_RD_INFO  ptr;
    ptr = kmalloc(sizeof(DEVICE_RD_INFO), GFP_ATOMIC);
    if (ptr == NULL)
        return NULL;
    else {
        memset(ptr,0,sizeof(DEVICE_RD_INFO));
        return ptr;
    }
}
*/

/*
typedef struct tagRDES0 {
    unsigned short wResCount;
    unsigned short wf1Owner ;
//    unsigned short f15Reserved : 15;
//    unsigned short f1Owner : 1;
} __attribute__ ((__packed__))
SRDES0;
*/

#ifdef __BIG_ENDIAN

typedef struct tagRDES0 {
   volatile unsigned short wResCount;
	union {
		volatile u16    f15Reserved;
		struct {
            volatile u8 f8Reserved1;
			volatile u8 f1Owner:1;
			volatile u8 f7Reserved:7;
		} __attribute__ ((__packed__));
	} __attribute__ ((__packed__));
} __attribute__ ((__packed__))
SRDES0, *PSRDES0;

#else

typedef struct tagRDES0 {
    unsigned short wResCount;
    unsigned short f15Reserved : 15;
    unsigned short f1Owner : 1;
} __attribute__ ((__packed__))
SRDES0;


#endif

typedef struct tagRDES1 {
    unsigned short wReqCount;
    unsigned short wReserved;
} __attribute__ ((__packed__))
SRDES1;

//
// Rx descriptor
//
typedef struct tagSRxDesc {
    volatile SRDES0 m_rd0RD0;
    volatile SRDES1 m_rd1RD1;
    volatile u32    buff_addr;
    volatile u32    next_desc;
    struct tagSRxDesc   *next;//4 bytes
    volatile PDEVICE_RD_INFO    pRDInfo;//4 bytes
    volatile u32    Reserved[2];//8 bytes
} __attribute__ ((__packed__))
SRxDesc, *PSRxDesc;
typedef const SRxDesc *PCSRxDesc;

#ifdef __BIG_ENDIAN

/*
typedef struct tagTDES0 {
    volatile    unsigned char byTSR0;
    volatile    unsigned char byTSR1;
    volatile    unsigned short wOwner_Txtime;
//    volatile    unsigned short f15Txtime : 15;
//    volatile    unsigned short f1Owner:1;
} __attribute__ ((__packed__))
STDES0;
*/

typedef struct tagTDES0 {
    volatile    unsigned char byTSR0;
    volatile    unsigned char byTSR1;
	union {
		volatile u16    f15Txtime;
		struct {
            volatile u8 f8Reserved1;
			volatile u8 f1Owner:1;
			volatile u8 f7Reserved:7;
		} __attribute__ ((__packed__));
	} __attribute__ ((__packed__));
} __attribute__ ((__packed__))
STDES0, PSTDES0;

#else

typedef struct tagTDES0 {
    volatile    unsigned char byTSR0;
    volatile    unsigned char byTSR1;
    volatile    unsigned short f15Txtime : 15;
    volatile    unsigned short f1Owner:1;
} __attribute__ ((__packed__))
STDES0;

#endif


typedef struct tagTDES1 {
    volatile    unsigned short wReqCount;
    volatile    unsigned char byTCR;
    volatile    unsigned char byReserved;
} __attribute__ ((__packed__))
STDES1;


typedef struct tagDEVICE_TD_INFO{
    struct sk_buff*     skb;
    unsigned char *buf;
    dma_addr_t          skb_dma;
    dma_addr_t          buf_dma;
    dma_addr_t          curr_desc;
    unsigned long dwReqCount;
    unsigned long dwHeaderLength;
    unsigned char byFlags;
} DEVICE_TD_INFO,    *PDEVICE_TD_INFO;

/*
static inline PDEVICE_TD_INFO alloc_td_info(void) {
    PDEVICE_TD_INFO  ptr;
    ptr = kmalloc(sizeof(DEVICE_TD_INFO),GFP_ATOMIC);
    if (ptr == NULL)
        return NULL;
    else {
        memset(ptr,0,sizeof(DEVICE_TD_INFO));
        return ptr;
    }
}
*/

//
// transmit descriptor
//
typedef struct tagSTxDesc {
    volatile    STDES0  m_td0TD0;
    volatile    STDES1  m_td1TD1;
    volatile    u32    buff_addr;
    volatile    u32    next_desc;
    struct tagSTxDesc*  next; //4 bytes
    volatile    PDEVICE_TD_INFO pTDInfo;//4 bytes
    volatile    u32    Reserved[2];//8 bytes
} __attribute__ ((__packed__))
STxDesc, *PSTxDesc;
typedef const STxDesc *PCSTxDesc;


typedef struct tagSTxSyncDesc {
    volatile    STDES0  m_td0TD0;
    volatile    STDES1  m_td1TD1;
    volatile    u32 buff_addr; // pointer to logical buffer
    volatile    u32 next_desc; // pointer to next logical descriptor
    volatile    unsigned short m_wFIFOCtl;
    volatile    unsigned short m_wTimeStamp;
    struct tagSTxSyncDesc*  next; //4 bytes
    volatile    PDEVICE_TD_INFO pTDInfo;//4 bytes
    volatile    u32 m_dwReserved2;
} __attribute__ ((__packed__))
STxSyncDesc, *PSTxSyncDesc;
typedef const STxSyncDesc *PCSTxSyncDesc;


//
// RsvTime buffer header
//
typedef struct tagSRrvTime_gRTS {
    unsigned short wRTSTxRrvTime_ba;
    unsigned short wRTSTxRrvTime_aa;
    unsigned short wRTSTxRrvTime_bb;
    unsigned short wReserved;
    unsigned short wTxRrvTime_b;
    unsigned short wTxRrvTime_a;
}__attribute__ ((__packed__))
SRrvTime_gRTS, *PSRrvTime_gRTS;
typedef const SRrvTime_gRTS *PCSRrvTime_gRTS;

typedef struct tagSRrvTime_gCTS {
    unsigned short wCTSTxRrvTime_ba;
    unsigned short wReserved;
    unsigned short wTxRrvTime_b;
    unsigned short wTxRrvTime_a;
}__attribute__ ((__packed__))
SRrvTime_gCTS, *PSRrvTime_gCTS;
typedef const SRrvTime_gCTS *PCSRrvTime_gCTS;

typedef struct tagSRrvTime_ab {
    unsigned short wRTSTxRrvTime;
    unsigned short wTxRrvTime;
}__attribute__ ((__packed__))
SRrvTime_ab, *PSRrvTime_ab;
typedef const SRrvTime_ab *PCSRrvTime_ab;

typedef struct tagSRrvTime_atim {
    unsigned short wCTSTxRrvTime_ba;
    unsigned short wTxRrvTime_a;
}__attribute__ ((__packed__))
SRrvTime_atim, *PSRrvTime_atim;
typedef const SRrvTime_atim *PCSRrvTime_atim;

//
// RTS buffer header
//
typedef struct tagSRTSData {
    unsigned short wFrameControl;
    unsigned short wDurationID;
    unsigned char abyRA[ETH_ALEN];
    unsigned char abyTA[ETH_ALEN];
}__attribute__ ((__packed__))
SRTSData, *PSRTSData;
typedef const SRTSData *PCSRTSData;

typedef struct tagSRTS_g {
    unsigned char bySignalField_b;
    unsigned char byServiceField_b;
    unsigned short wTransmitLength_b;
    unsigned char bySignalField_a;
    unsigned char byServiceField_a;
    unsigned short wTransmitLength_a;
    unsigned short wDuration_ba;
    unsigned short wDuration_aa;
    unsigned short wDuration_bb;
    unsigned short wReserved;
    SRTSData    Data;
}__attribute__ ((__packed__))
SRTS_g, *PSRTS_g;
typedef const SRTS_g *PCSRTS_g;


typedef struct tagSRTS_g_FB {
    unsigned char bySignalField_b;
    unsigned char byServiceField_b;
    unsigned short wTransmitLength_b;
    unsigned char bySignalField_a;
    unsigned char byServiceField_a;
    unsigned short wTransmitLength_a;
    unsigned short wDuration_ba;
    unsigned short wDuration_aa;
    unsigned short wDuration_bb;
    unsigned short wReserved;
    unsigned short wRTSDuration_ba_f0;
    unsigned short wRTSDuration_aa_f0;
    unsigned short wRTSDuration_ba_f1;
    unsigned short wRTSDuration_aa_f1;
    SRTSData    Data;
}__attribute__ ((__packed__))
SRTS_g_FB, *PSRTS_g_FB;
typedef const SRTS_g_FB *PCSRTS_g_FB;


typedef struct tagSRTS_ab {
    unsigned char bySignalField;
    unsigned char byServiceField;
    unsigned short wTransmitLength;
    unsigned short wDuration;
    unsigned short wReserved;
    SRTSData    Data;
}__attribute__ ((__packed__))
SRTS_ab, *PSRTS_ab;
typedef const SRTS_ab *PCSRTS_ab;


typedef struct tagSRTS_a_FB {
    unsigned char bySignalField;
    unsigned char byServiceField;
    unsigned short wTransmitLength;
    unsigned short wDuration;
    unsigned short wReserved;
    unsigned short wRTSDuration_f0;
    unsigned short wRTSDuration_f1;
    SRTSData    Data;
}__attribute__ ((__packed__))
SRTS_a_FB, *PSRTS_a_FB;
typedef const SRTS_a_FB *PCSRTS_a_FB;


//
// CTS buffer header
//
typedef struct tagSCTSData {
    unsigned short wFrameControl;
    unsigned short wDurationID;
    unsigned char abyRA[ETH_ALEN];
    unsigned short wReserved;
}__attribute__ ((__packed__))
SCTSData, *PSCTSData;

typedef struct tagSCTS {
    unsigned char bySignalField_b;
    unsigned char byServiceField_b;
    unsigned short wTransmitLength_b;
    unsigned short wDuration_ba;
    unsigned short wReserved;
    SCTSData    Data;
}__attribute__ ((__packed__))
SCTS, *PSCTS;
typedef const SCTS *PCSCTS;

typedef struct tagSCTS_FB {
    unsigned char bySignalField_b;
    unsigned char byServiceField_b;
    unsigned short wTransmitLength_b;
    unsigned short wDuration_ba;
    unsigned short wReserved;
    unsigned short wCTSDuration_ba_f0;
    unsigned short wCTSDuration_ba_f1;
    SCTSData    Data;
}__attribute__ ((__packed__))
SCTS_FB, *PSCTS_FB;
typedef const SCTS_FB *PCSCTS_FB;


//
// Tx FIFO header
//
typedef struct tagSTxBufHead {
    u32 adwTxKey[4];
    unsigned short wFIFOCtl;
    unsigned short wTimeStamp;
    unsigned short wFragCtl;
    unsigned char byTxPower;
    unsigned char wReserved;
}__attribute__ ((__packed__))
STxBufHead, *PSTxBufHead;
typedef const STxBufHead *PCSTxBufHead;

typedef struct tagSTxShortBufHead {
    unsigned short wFIFOCtl;
    unsigned short wTimeStamp;
}__attribute__ ((__packed__))
STxShortBufHead, *PSTxShortBufHead;
typedef const STxShortBufHead *PCSTxShortBufHead;

//
// Tx data header
//
typedef struct tagSTxDataHead_g {
    unsigned char bySignalField_b;
    unsigned char byServiceField_b;
    unsigned short wTransmitLength_b;
    unsigned char bySignalField_a;
    unsigned char byServiceField_a;
    unsigned short wTransmitLength_a;
    unsigned short wDuration_b;
    unsigned short wDuration_a;
    unsigned short wTimeStampOff_b;
    unsigned short wTimeStampOff_a;
}__attribute__ ((__packed__))
STxDataHead_g, *PSTxDataHead_g;
typedef const STxDataHead_g *PCSTxDataHead_g;

typedef struct tagSTxDataHead_g_FB {
    unsigned char bySignalField_b;
    unsigned char byServiceField_b;
    unsigned short wTransmitLength_b;
    unsigned char bySignalField_a;
    unsigned char byServiceField_a;
    unsigned short wTransmitLength_a;
    unsigned short wDuration_b;
    unsigned short wDuration_a;
    unsigned short wDuration_a_f0;
    unsigned short wDuration_a_f1;
    unsigned short wTimeStampOff_b;
    unsigned short wTimeStampOff_a;
}__attribute__ ((__packed__))
STxDataHead_g_FB, *PSTxDataHead_g_FB;
typedef const STxDataHead_g_FB *PCSTxDataHead_g_FB;


typedef struct tagSTxDataHead_ab {
    unsigned char bySignalField;
    unsigned char byServiceField;
    unsigned short wTransmitLength;
    unsigned short wDuration;
    unsigned short wTimeStampOff;
}__attribute__ ((__packed__))
STxDataHead_ab, *PSTxDataHead_ab;
typedef const STxDataHead_ab *PCSTxDataHead_ab;


typedef struct tagSTxDataHead_a_FB {
    unsigned char bySignalField;
    unsigned char byServiceField;
    unsigned short wTransmitLength;
    unsigned short wDuration;
    unsigned short wTimeStampOff;
    unsigned short wDuration_f0;
    unsigned short wDuration_f1;
}__attribute__ ((__packed__))
STxDataHead_a_FB, *PSTxDataHead_a_FB;
typedef const STxDataHead_a_FB *PCSTxDataHead_a_FB;

//
// MICHDR data header
//
typedef struct tagSMICHDRHead {
    u32 adwHDR0[4];
    u32 adwHDR1[4];
    u32 adwHDR2[4];
}__attribute__ ((__packed__))
SMICHDRHead, *PSMICHDRHead;
typedef const SMICHDRHead *PCSMICHDRHead;

typedef struct tagSBEACONCtl {
    u32 BufReady : 1;
    u32 TSF      : 15;
    u32 BufLen   : 11;
    u32 Reserved : 5;
}__attribute__ ((__packed__))
SBEACONCtl;


typedef struct tagSSecretKey {
    u32 dwLowDword;
    unsigned char byHighByte;
}__attribute__ ((__packed__))
SSecretKey;

typedef struct tagSKeyEntry {
    unsigned char abyAddrHi[2];
    unsigned short wKCTL;
    unsigned char abyAddrLo[4];
    u32 dwKey0[4];
    u32 dwKey1[4];
    u32 dwKey2[4];
    u32 dwKey3[4];
    u32 dwKey4[4];
}__attribute__ ((__packed__))
SKeyEntry;
/*---------------------  Export Macros ------------------------------*/

/*---------------------  Export Classes  ----------------------------*/

/*---------------------  Export Variables  --------------------------*/

/*---------------------  Export Functions  --------------------------*/




#endif // __DESC_H__
