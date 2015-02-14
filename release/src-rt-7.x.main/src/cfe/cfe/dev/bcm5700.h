/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM570X (10/100/1K EthernetMAC) registers       File: bcm5700.c
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

#ifndef _BCM5700_H_
#define _BCM5700_H_

/*
 * Register and bit definitions for the Broadcom BCM570X family (aka
 * Tigon3) of Integrated MACs.
 *
 * The 5700 option for external SSRAM is ignored, since no subsequent
 * chips support it, nor do the 5700 evaluation boards.
 *
 * References:
 * 
 *   Host Programmer Interface Specification for the BCM570X Family
 *     of Highly-Integrated Media Access Controllers, 570X-PG106-R.
 *   Broadcom Corp., 16215 Alton Parkway, Irvine CA, 09/27/02
 *
 *   Simplified Programmer Interface Specification for the BCM570X Family
 *     of Highly Integrated Media Access Controllers, 570X-PG202-R.
 *   Broadcom Corp., 16215 Alton Parkway, Irvine CA, 10/14/02
 */

#define K_PCI_VENDOR_BROADCOM  0x14e4
#define K_PCI_ID_BCM5700       0x1644
#define K_PCI_ID_BCM5701       0x1645
#define K_PCI_ID_BCM5702       0x16A6
#define K_PCI_ID_BCM5703       0x1647
#define K_PCI_ID_BCM5703a      0x16A7
#define K_PCI_ID_BCM5703b      0x16C7
#define K_PCI_ID_BCM5704C      0x1648
#define K_PCI_ID_BCM5704S      0x16A8
#define K_PCI_ID_BCM5705       0x1653
#define K_PCI_ID_BCM5750       0x1676
#define K_PCI_ID_BCM471F       0x471F

#define K_PCI_ID_BCM5705_OR_BCM5750(_coreID) \
	((_coreID == K_PCI_ID_BCM5705) || \
	 (_coreID == K_PCI_ID_BCM5750))

#define _DD_MAKEMASK1(n) (1 << (n))
#define _DD_MAKEMASK(v,n) ((((1)<<(v))-1) << (n))
#define _DD_MAKEVALUE(v,n) ((v) << (n))
#define _DD_GETVALUE(v,n,m) (((v) & (m)) >> (n))


/* Registers 0x0000 - 0x00FF are PCI Configuration registers (shadow) */

#define PCI_PCIX_CMD_REG        0x40
#define PCI_PCIX_STAT_REG       0x44

#define PCI_PMC_REG             0x48
#define PCI_PMCSR_REG           0x4C

#define PCI_VPD_CAP_REG         0x50
#define PCI_VPD_DATA_REG        0x54

#define PCI_MSI_CTL_REG         0x58
#define PCI_MSI_ADDR_REG        0x5C     /* 8 bytes */
#define PCI_MSI_DATA_REG        0x64

#define R_MISC_HOST_CTRL        0x0068
#define R_DMA_RW_CTRL           0x006C
#define R_PCI_STATE             0x0070
#define R_PCI_CLK_CTRL          0x0074
#define R_REG_BASE_ADDR         0x0078
#define R_MEMWIN_BASE_ADDR      0x007C
#define R_REG_DATA              0x0080
#define R_MEMWIN_DATA           0x0084
/* For 5700 and 5701, 0x0088-0x0090 shadow 0x6800-0x6808 */
#define R_INT_MBOX0             0x00B0   /* 8 bytes, shadows R_INT_MBOX(0) */

/* Registers 0x0200 - 0x03FF are High Priority Mailbox registers */

#define R_INT_MBOX(n)           (0x0200 + 8*(n))    /* 0 <= n < 4 */
#define R_GEN_MBOX(n)           (0x0220 + 8*(n)-8)  /* 1 <= n <= 8 */
#define R_RELOAD_STATS_MBOX     0x0260
#define R_RCV_BD_STD_PI         0x0268
#define R_RCV_BD_JUMBO_PI       0x0270
#define R_RCV_BD_MINI_PI        0x0278
#define R_RCV_BD_RTN_CI(n)      (0x0280 + 8*(n)-8)  /* 1 <= n <= 16 */
#define R_SND_BD_PI(n)          (0x0300 + 8*(n)-8)  /* 1 <= n <= 16 */
#define R_SND_BD_NIC_PI(n)      (0x0380 + 8*(n)-8)  /* 1 <= n <= 16 */

/* Registers 0x0400 - 0x0BFF are MAC Control registers */

#define R_MAC_MODE              0x0400
#define R_MAC_STATUS            0x0404
#define R_MAC_EVENT_ENB         0x0408
#define R_MAC_LED_CTRL          0x040C

#define R_MAC_ADDR1_HIGH        0x0410
#define R_MAC_ADDR1_LOW         0x0414
#define R_MAC_ADDR2_HIGH        0x0418
#define R_MAC_ADDR2_LOW         0x041C
#define R_MAC_ADDR3_HIGH        0x0420
#define R_MAC_ADDR3_LOW         0x0424
#define R_MAC_ADDR4_HIGH        0x0428
#define R_MAC_ADDR4_LOW         0x042C

#define R_WOL_PATTERN_PTR       0x0430
#define R_WOL_PATTERN_CFG       0x0434
#define R_TX_BACKOFF            0x0438
#define R_RX_MTU                0x043C
#define R_PCS_TEST              0x0440
#define R_TX_AUTONEG            0x0444
#define R_RX_AUTONEG            0x0448

#define R_MI_COMM               0x044C
#define R_MI_STATUS             0x0450
#define R_MI_MODE               0x0454

#define R_AUTOPOLL_STAT         0x0458
#define R_TX_MODE               0x045C
#define R_TX_STAT               0x0460
#define R_TX_LENS               0x0464
#define R_RX_MODE               0x0468
#define R_RX_STAT               0x046C

#define R_MAC_HASH(n)           (0x0470 + 4*(n))    /* 0 <= n < 4 */

#define R_RX_BD_RULES_CTRL(n)   (0x0480 + 8*(n))    /* 0 <= n < 16 */
#define R_RX_BD_RULES_MASK(n)   (0x0484 + 8*(n))

#define R_RX_RULES_CFG          0x0500
#define R_RX_FRAMES_LOW         0x0504
#define R_MAC_HASH_EXT(n)       (0x0520 + 4*(n))    /* 0 <= n < 4 */
#define R_MAC_ADDR_EXT(n)       (0x0530 + 8*(n))    /* 0 <= n < 12 */
#define R_SERDES_CTRL           0x0590
#define R_SERDES_STAT           0x0594
#define R_RX_STATS_MEM          0x0800
#define R_TX_STATS_MEM          0x0880


/*
 * Note on Buffer Descriptor (BD) Ring indices:
 * Numbering follows Broadcom literature, which uses indices 1-16.
 * PI = producer index, CI = consumer index.
 */

/* Registers 0x0C00 - 0x0FFF are Send Data Initiator Control registers */

#define R_SND_DATA_MODE         0x0C00
#define R_SND_DATA_STAT         0x0C04
#define R_SND_DATA_STATS_CTRL   0x0C08
#define R_SND_DATA_STATS_ENB    0x0C0C
#define R_SND_DATA_STATS_INCR   0x0C10

#define R_STATS_CTR_SND_COS(n)  (0x0C80 + 4*(n)-4)  /* 1 <= n <= 16 */
#define R_STATS_DMA_RDQ_FULL    0x0CC0
#define R_STATS_DMA_HP_RDQ_FULL 0x0CC4
#define R_STATS_SDCQ_FULL       0x0CC8
#define R_STATS_NIC_SET_SND_PI  0x0CCC
#define R_STATS_STAT_UPDATED    0x0CD0
#define R_STATS_IRQS            0x0CD4
#define R_STATS_IRQS_AVOIDED    0x0CD8
#define R_STATS_SND_THRSH_HIT   0x0CDC

/* Registers 0x1000 - 0x13FF are Send Data Completion Control registers */

#define R_SND_DATA_COMP_MODE    0x1000

/* Registers 0x1400 - 0x17FF are Send BD Ring Selection Control registers */

#define R_SND_BD_SEL_MODE       0x1400
#define R_SND_BD_SEL_STAT       0x1404
#define R_SND_BD_DIAG           0x1408
#define R_SND_BD_SEL_CI(n)      (0x1440 + 4*(n)-4)  /* 1 <= n <= 16  */

/* Registers 0x1800 - 0x1BFF are Send BD Initiator Control registers */

#define R_SND_BD_INIT_MODE      0x1800
#define R_SND_BD_INIT_STAT      0x1804
#define R_SND_BD_INIT_PI(n)     (0x1808 + 4*(n)-4)  /* 1 <= n <= 16 */

/* Registers 0x1C00 - 0x1FFF are Send BD Completion Control registers */

#define R_SND_BD_COMP_MODE      0x1C00

/* Registers 0x2000 - 0x23FF are Receive List Placement Control registers */

#define R_RCV_LIST_MODE         0x2000
#define R_RCV_LIST_STAT         0x2004
#define R_RCV_LIST_LOCK         0x2008
#define R_RCV_NONEMPTY_BITS     0x200C
#define R_RCV_LIST_CFG          0x2010
#define R_RCV_LIST_STATS_CTRL   0x2014
#define R_RCV_LIST_STATS_ENB    0x2018
#define R_RCV_LIST_STATS_INC    0x201C
#define R_RCV_LIST_HEAD(n)      (0x2100 + 16*(n)-16) /* 1 <= n <= 16 */
#define R_RCV_LIST_TAIL(n)      (0x2104 + 16*(n)-16)
#define R_RCV_LIST_CNT(n)       (0x2108 + 16*(n)-16)
#define R_STATS_CTR_RCV_COS(n)  (0x2200 + 4*(n)-4)   /* 1 <= n <= 16 */
#define R_STATS_FILT_DROP       0x2240
#define R_STATS_DMA_WRQ_FULL    0x2244
#define R_STATS_DMA_HP_WRQ_FULL 0x2248
#define R_STATS_NO_RCV_BDS      0x224C
#define R_STATS_IN_DISCARDS     0x2250
#define R_STATS_IN_ERRORS       0x2254
#define R_STATS_RCV_THRSH_HIT   0x2258

/* Registers 0x2400 - 0x27FF are Receive Data/BD Initiator Control registers */

#define R_RCV_DATA_INIT_MODE    0x2400
#define R_RCV_DATA_INIT_STAT    0x2404
#define R_JUMBO_RCV_BD_RCB      0x2440              /* 16 bytes */
#define R_STD_RCV_BD_RCB        0x2450              /* 16 bytes */
#define R_MINI_RCV_BD_RCB       0x2460              /* 16 bytes */
#define R_RCV_BD_INIT_JUMBO_CI  0x2470
#define R_RCV_BD_INIT_STD_CI    0x2474
#define R_RCV_BD_INIT_MINI_CI   0x2478
#define R_RCV_BD_INIT_RTN_PI(n) (0x2480 + 4*(n)-4)  /* 1 <= n <= 16 */
#define R_RCV_BD_INIT_DIAG      0x24C0

/* Registers 0x2800 - 0x2BFF are Receive Data Completion Control registers */

#define R_RCV_COMP_MODE         0x2800

/* Registers 0x2C00 - 0x2FFF are Receive BD Initiator Control registers */

#define R_RCV_BD_INIT_MODE      0x2C00
#define R_RCV_BD_INIT_STAT      0x2C04
#define R_RCV_BD_INIT_JUMBO_PI  0x2C08
#define R_RCV_BD_INIT_STD_PI    0x2C0C
#define R_RCV_BD_INIT_MINI_PI   0x2C10
#define R_MINI_RCV_BD_THRESH    0x2C14
#define R_STD_RCV_BD_THRESH     0x2C18
#define R_JUMBO_RCV_BD_THRESH   0x2C1C

/* Registers 0x3000 - 0x33FF are Receive BD Completion Control registers */

#define R_RCV_BD_COMP_MODE      0x3000
#define R_RCV_BD_COMP_STAT      0x3004
#define R_NIC_JUMBO_RCV_BD_PI   0x3008
#define R_NIC_STD_RCV_BD_PI     0x300C
#define R_NIC_MINI_RCV_BD_PI    0x3010

/* Registers 0x3400 - 0x37FF are Receive List Selector Control registers */

#define R_RCV_LIST_SEL_MODE     0x3400
#define R_RCV_LIST_SEL_STATUS   0x3404

/* Registers 0x3800 - 0x3BFF are Mbuf Cluster Free registers */

#define R_MBUF_FREE_MODE        0x3800
#define R_MBUF_FREE_STATUS      0x3804

/* Registers 0x3C00 - 0x3FFF are Host Coalescing Control registers */

#define R_HOST_COAL_MODE        0x3C00
#define R_HOST_COAL_STAT        0x3C04
#define R_RCV_COAL_TICKS        0x3C08
#define R_SND_COAL_TICKS        0x3C0C
#define R_RCV_COAL_MAX_CNT      0x3C10
#define R_SND_COAL_MAX_CNT      0x3C14
#define R_RCV_COAL_INT_TICKS    0x3C18
#define R_SND_COAL_INT_TICKS    0x3C1C
#define R_RCV_COAL_INT_CNT      0x3C20
#define R_SND_COAL_INT_CNT      0x3C24
#define R_STATS_TICKS           0x3C28
#define R_STATS_HOST_ADDR       0x3C30     /* 8 bytes */
#define R_STATUS_HOST_ADDR      0x3C38     /* 8 bytes */
#define R_STATS_BASE_ADDR       0x3C40
#define R_STATUS_BASE_ADDR      0x3C44
#define R_FLOW_ATTN             0x3C48
#define R_NIC_JUMBO_RCV_BD_CI   0x3C50
#define R_NIC_STD_RCV_BD_CI     0x3C54
#define R_NIC_MINI_RCV_BD_CI    0x3C58
#define R_NIC_RTN_PI(n)         (0x3C80 + 4*(n)-4)  /* 1 <= n <= 16 */
#define R_NIC_SND_BD_CD(n)      (0x3CC0 + 4*(n)-4)  /* 1 <= n <= 16 */

/* Registers 0x4000 - 0x43FF are Memory Arbiter registers */

#define R_MEM_MODE              0x4000
#define R_MEM_STATUS            0x4004
#define R_MEM_TRAP_LOW          0x4008
#define R_MEM_TRAP_HIGH         0x400C


/* Registers 0x4400 - 0x47FF are Buffer Manager Control registers */

#define R_BMGR_MODE             0x4400
#define R_BMGR_STATUS           0x4404
#define R_BMGR_MBUF_BASE        0x4408
#define R_BMGR_MBUF_LEN         0x440C
#define R_BMGR_MBUF_DMA_LOW     0x4410
#define R_BMGR_MBUF_RX_LOW      0x4414
#define R_BMGR_MBUF_HIGH        0x4418

#define R_BMGR_DMA_BASE         0x442C
#define R_BMGR_DMA_LEN          0x4430
#define R_BMGR_DMA_LOW          0x4434
#define R_BMGR_DMA_HIGH         0x4438

#define R_BMGR_DIAG1            0x444C
#define R_BMGR_DIAG2            0x4450
#define R_BMGR_DIAG3            0x4454
#define R_BMGR_RCV_FLOW_THRESH  0x4458

/* Registers 0x4800 - 0x4BFF are Read DMA Control registers */

#define R_RD_DMA_MODE           0x4800
#define R_RD_DMA_STAT           0x4804

/* Registers 0x4C00 - 0x4FFF are Write DMA Control registers */

#define R_WR_DMA_MODE           0x4C00
#define R_WR_DMA_STAT           0x4C04

/* Registers 0x5000 - 0x53FF are RX RISC registers */

#define R_RX_RISC_MODE          0x5000
#define R_RX_RISC_STATE         0x5004
#define R_RX_RISC_PC            0x501C

/* Registers 0x5400 - 0x57FF are TX RISC registers */

#define R_TX_RISC_MODE          0x5400
#define R_TX_RISC_STATE         0x5404
#define R_TX_RISC_PC            0x541C

/* Registers 0x5800 - 0x5BFF are Low Priority Mailbox registers (8 bytes) */

#define R_LP_INT_MBOX(n)        (0x5800 + 8*(n))    /* 0 <= n < 4 */
#define R_LP_GEN_MBOX(n)        (0x5820 + 8*(n)-8)  /* 1 <= n <= 8 */
#define R_LP_RELOAD_STATS_MBOX  0x5860
#define R_LP_RCV_BD_STD_PI      0x5868
#define R_LP_RCV_BD_JUMBO_PI    0x5870
#define R_LP_RCV_BD_MINI_PI     0x5878
#define R_LP_RCV_BD_RTN_CI(n)   (0x5880 + 8*(n)-8)  /* 1 <= n <= 16 */
#define R_LP_SND_BD_PI(n)       (0x5900 + 8*(n)-8)  /* 1 <= n <= 16 */

/* Registers 0x5C00 - 0x5C03 are Flow Through Queues */

#define R_FTQ_RESET             0x5C00

/* Registers 0x6000 - 0x63FF are Message Signaled Interrupt registers */

#define R_MSI_MODE              0x6000
#define R_MSI_STATUS            0x6004
#define R_MSI_FIFO_ACCESS       0x6008

/* Registers 0x6400 - 0x67FF are DMA Completion registers */

#define R_DMA_COMP_MODE         0x6400

/* Registers 0x6800 - 0x68ff are General Control registers */

#define R_MODE_CTRL             0x6800
#define R_MISC_CFG              0x6804
#define R_MISC_LOCAL_CTRL       0x6808
#define R_TIMER                 0x680C
#define R_MEM_PWRUP             0x6030        /* 8 bytes */
#define R_EEPROM_ADDR           0x6838
#define R_EEPROM_DATA           0x683C
#define R_EEPROM_CTRL           0x6840
#define R_MDI_CTRL              0x6844
#define R_EEPROM_DELAY          0x6848

/* Registers 0x6C00 - 0x6CFF are ASF Support registers (NYI) */

/* Registers 0x7000 - 0x7024 are NVM Interface registers (NYI) */


/* PCI Capability registers (should be moved to PCI headers) */

/* PCI-X Capability and Command Register (0x40) */

#define PCIX_CMD_DPREC_ENABLE                 0x00010000
#define PCIX_CMD_RLXORDER_ENABLE              0x00020000
#define PCIX_CMD_RD_CNT_SHIFT                 18
#define PCIX_CMD_RD_CNT_MASK                  0x000C0000
#define PCIX_CMD_MAX_SPLIT_SHIFT              20
#define PCIX_CMD_MAX_SPLIT_MASK               0x00700000

/* PCI-X Status Register (0x44) */


/* Generic bit fields shared by most MODE and STATUS registers */

#define M_MODE_RESET            _DD_MAKEMASK1(0)
#define M_MODE_ENABLE           _DD_MAKEMASK1(1)
#define M_MODE_ATTNENABLE       _DD_MAKEMASK1(2)

#define M_STAT_ERROR            _DD_MAKEMASK1(1)

/* Generic bit fields shared by STATS_CTRL registers */

#define M_STATS_ENABLE          _DD_MAKEMASK1(0)
#define M_STATS_FASTUPDATE      _DD_MAKEMASK1(1)
#define M_STATS_CLEAR           _DD_MAKEMASK1(2)
#define M_STATS_FLUSH           _DD_MAKEMASK1(3)
#define M_STATS_ZERO            _DD_MAKEMASK1(4)

/* Private PCI Configuration registers (p 335) */

/* MHC: Miscellaneous Host Control Register (0x68) */

#define M_MHC_CLEARINTA         _DD_MAKEMASK1(0)
#define M_MHC_MASKPCIINT        _DD_MAKEMASK1(1)
#define M_MHC_ENBYTESWAP        _DD_MAKEMASK1(2)
#define M_MHC_ENWORDSWAP        _DD_MAKEMASK1(3)
#define M_MHC_ENPCISTATERW      _DD_MAKEMASK1(4)
#define M_MHC_ENCLKCTRLRW       _DD_MAKEMASK1(5)
#define M_MHC_ENREGWORDSWAP     _DD_MAKEMASK1(6)
#define M_MHC_ENINDIRECT        _DD_MAKEMASK1(7)
#define S_MHC_ASICREV           16
#define M_MHC_ASICREV           _DD_MAKEMASK(16,S_MHC_ASICREV)
#define G_MHC_ASICREV(x)        _DD_GETVALUE(x,S_MHC_ASICREV,M_MHC_ASICREV)

/* DMAC: DMA Read/Write Control Register (0x6c) */

#define S_DMAC_MINDMA           0
#define M_DMAC_MINDMA           _DD_MAKEMASK(8,S_DMAC_MINDMA)
#define V_DMAC_MINDMA(x)        _DD_MAKEVALUE(x,S_DMAC_MINDMA)
#define G_DMAC_MINDMA(x)        _DD_GETVALUE(x,S_DMAC_MINDMA,M_DMAC_MINDMA)

#define S_DMAC_RDBDY            8                   /* 570{0,1} only */
#define M_DMAC_RDBDY            _DD_MAKEMASK(3,S_DMAC_RDBDY)
#define V_DMAC_RDBDY(x)         _DD_MAKEVALUE(x,S_DMAC_RDBDY)
#define G_DMAC_RDBDY(x)         _DD_GETVALUE(x,S_DMAC_RDBDY,M_DMAC_RDBDY)

#define S_DMAC_WRBDY            11                  /* 570{0,1} only */
#define M_DMAC_WRBDY            _DD_MAKEMASK(3,S_DMAC_WRBDY)
#define V_DMAC_WRBDY(x)         _DD_MAKEVALUE(x,S_DMAC_WRBDY)
#define G_DMAC_WRBDY(x)         _DD_GETVALUE(x,S_DMAC_WRBDY,M_DMAC_WRBDY)

#define S_DMAC_ONEDMA           14
#define M_DMAC_ONEDMA           _DD_MAKEMASK(2,S_DMAC_ONEDMA)
#define V_DMAC_ONEDMA(x)        _DD_MAKEVALUE(x,S_DMAC_ONEDMA)
#define G_DMAC_ONEDMA(x)        _DD_GETVALUE(x,S_DMAC_ONEDMA,M_DMAC_WRBDY)

#define S_DMAC_RDWMK            16
#define M_DMAC_RDWMK            _DD_MAKEMASK(3,S_DMAC_RDWMK)
#define V_DMAC_RDWMK(x)         _DD_MAKEVALUE(x,S_DMAC_RDWMK)
#define G_DMAC_RDWMK(x)         _DD_GETVALUE(x,S_DMAC_RDWMK,M_DMAC_RDWMK)

#define S_DMAC_WRWMK            19
#define M_DMAC_WRWMK            _DD_MAKEMASK(3,S_DMAC_WRWMK)
#define V_DMAC_WRWMK(x)         _DD_MAKEVALUE(x,S_DMAC_WRWMK)
#define G_DMAC_WRWMK(x)         _DD_GETVALUE(x,S_DMAC_WRWMK,M_DMAC_WRWMK)

#define M_DMAC_MEMRDMULT        _DD_MAKEMASK1(22)   /* 570{0,1} only */
#define M_DMAC_BEALL            _DD_MAKEMASK1(23)   /* 570{0,1} only */
#define M_DMAC_ENBUGFIX         _DD_MAKEMASK1(23)   /* 570{3,4} only */

#define S_DMAC_RDCMD            24                  /* 570{0,1} only */
#define M_DMAC_RDCMD            _DD_MAKEMASK(4,S_DMAC_RDCMD)
#define V_DMAC_RDCMD(x)         _DD_MAKEVALUE(x,S_DMAC_RDCMD)
#define G_DMAC_RDCMD(x)         _DD_GETVALUE(x,S_DMAC_RDCMD,M_DMAC_RDCMD)
#define K_PCI_MEMRD             0x6

#define S_DMAC_WRCMD            28
#define M_DMAC_WRCMD            _DD_MAKEMASK(4,S_DMAC_WRCMD)
#define V_DMAC_WRCMD(x)         _DD_MAKEVALUE(x,S_DMAC_WRCMD)
#define G_DMAC_WRCMD(x)         _DD_GETVALUE(x,S_DMAC_WRCMD,M_DMAC_WRCMD)
#define K_PCI_MEMWR             0x7

/* PCIS: PCI State Register (0x70) */

#define M_PCIS_RESET            _DD_MAKEMASK1(0)
#define M_PCIS_INT              _DD_MAKEMASK1(1)
#define M_PCIS_MODE             _DD_MAKEMASK1(2)
#define M_PCIS_33MHZ            _DD_MAKEMASK1(3)
#define M_PCIS_32BIT            _DD_MAKEMASK1(4)
#define M_PCIS_ROMEN            _DD_MAKEMASK1(5)
#define M_PCIS_ROMRETRY         _DD_MAKEMASK1(6)
#define M_PCIS_FLATVIEW         _DD_MAKEMASK1(8)

#define S_PCIS_MAXRETRY         9                   /* 570{0,1} only */
#define M_PCIS_MAXRETRY         _DD_MAKEMASK(3,S_PCIS_MAXRETRY)
#define V_PCIS_MAXRETRY(x)      _DD_MAKEVALUE(x,S_PCIS_MAXRETRY)
#define G_PCIS_MAXRETRY(x)      _DD_GETVALUE(x,S_PCIS_MAXRETRY,M_PCIS_MAXRETRY)

#define M_PCIS_RETRYSAME        _DD_MAKEMASK1(13)   /* not 570{0,1} */
#define M_PCIS_CARDBUSMODE      _DD_MAKEMASK1(14)   /* 5705 only */
#define M_PCIS_FORCERETRY       _DD_MAKEMASK1(15)   /* 5705 only */

/* PCI Clock Control Register (0x74) */

/* Register Base Address Register (0x78) */

/* Memory Window Base Address Register (0x7c) */


/* High Priority Mailboxes (p 323) */


/* Ethernet MAC Control registers (p 358) */

/* MACM: Ethernet MAC Mode Register (0x400) */

#define M_MACM_GLBRESET         _DD_MAKEMASK1(0)
#define M_MACM_HALFDUPLEX       _DD_MAKEMASK1(1)

#define S_MACM_PORTMODE         2
#define M_MACM_PORTMODE         _DD_MAKEMASK(2,S_MACM_PORTMODE)
#define V_MACM_PORTMODE(x)      _DD_MAKEVALUE(x,S_MACM_PORTMODE)
#define G_MACM_PORTMODE(x)      _DD_GETVALUE(x,S_MACM_PORTMODE,M_MACM_PORTMODE)
#define K_MACM_PORTMODE_NONE    0x0
#define K_MACM_PORTMODE_MII     0x1
#define K_MACM_PORTMODE_GMII    0x2
#define K_MACM_PORTMODE_TBI     0x3

#define M_MACM_LOOPBACK         _DD_MAKEMASK1(4)
#define M_MACM_TAGGEDMAC        _DD_MAKEMASK1(7)
#define M_MACM_TXBURST          _DD_MAKEMASK1(8)
#define M_MACM_MAXDEFER         _DD_MAKEMASK1(9)
#define M_MACM_LINKPOLARITY     _DD_MAKEMASK1(10)
#define M_MACM_RXSTATSENB       _DD_MAKEMASK1(11)
#define M_MACM_RXSTATSCLR       _DD_MAKEMASK1(12)
#define M_MACM_RXSTATSFLUSH     _DD_MAKEMASK1(13)
#define M_MACM_TXSTATSENB       _DD_MAKEMASK1(14)
#define M_MACM_TXSTATSCLR       _DD_MAKEMASK1(15)
#define M_MACM_TXSTATSFLUSH     _DD_MAKEMASK1(16)
#define M_MACM_SENDCFGS         _DD_MAKEMASK1(17)
#define M_MACM_MAGICPKT         _DD_MAKEMASK1(18)
#define M_MACM_ACPI             _DD_MAKEMASK1(19)
#define M_MACM_MIPENB           _DD_MAKEMASK1(20)
#define M_MACM_TDEENB           _DD_MAKEMASK1(21)
#define M_MACM_RDEENB           _DD_MAKEMASK1(22)
#define M_MACM_FHDEENB          _DD_MAKEMASK1(23)

/* MACSTAT: Ethernet MAC Status Register (0x404) */
/* MACEVNT:  Ethernet MAC Event Enable Register (0x408) */

/* Status Register only */
#define M_MACSTAT_PCSSYNC       _DD_MAKEMASK1(0)
#define M_MACSTAT_SIGDET        _DD_MAKEMASK1(1)
#define M_MACSTAT_RCVCFG        _DD_MAKEMASK1(2)
#define M_MACSTAT_CFGCHNG       _DD_MAKEMASK1(3)
#define M_MACSTAT_SYNCCHNG      _DD_MAKEMASK1(4)
/* Status and Enable Registers */
#define M_EVT_PORTERR           _DD_MAKEMASK1(10)
#define M_EVT_LINKCHNG          _DD_MAKEMASK1(12)
#define M_EVT_MICOMPLETE        _DD_MAKEMASK1(22)
#define M_EVT_MIINT             _DD_MAKEMASK1(23)
#define M_EVT_APERR             _DD_MAKEMASK1(24)
#define M_EVT_ODIERR            _DD_MAKEMASK1(25)
#define M_EVT_RXSTATOVRUN       _DD_MAKEMASK1(26)
#define M_EVT_TXSTATOVRUN       _DD_MAKEMASK1(27)


/* LEDCTRL: LED Control Register (0x40c) */
#define M_LEDCTRL_OVERRIDE		_DD_MAKEMASK1(0)
#define M_LEDCTRL_1000MBPS		_DD_MAKEMASK1(1)
#define M_LEDCTRL_100MBPS		_DD_MAKEMASK1(2)


/* MICOMM: MI Communication Register (0x44c) */

#define S_MICOMM_DATA           0
#define M_MICOMM_DATA           _DD_MAKEMASK(16,S_MICOMM_DATA)
#define V_MICOMM_DATA(x)        _DD_MAKEVALUE(x,S_MICOMM_DATA)
#define G_MICOMM_DATA(x)        _DD_GETVALUE(x,S_MICOMM_DATA,M_MICOMM_DATA)

#define S_MICOMM_REG            16
#define M_MICOMM_REG            _DD_MAKEMASK(5,S_MICOMM_REG)
#define V_MICOMM_REG(x)         _DD_MAKEVALUE(x,S_MICOMM_REG)
#define G_MICOMM_REG(x)         _DD_GETVALUE(x,S_MICOMM_REG,M_MICOMM_REG)

#define S_MICOMM_PHY            21
#define M_MICOMM_PHY            _DD_MAKEMASK(5,S_MICOMM_PHY)
#define V_MICOMM_PHY(x)         _DD_MAKEVALUE(x,S_MICOMM_PHY)
#define G_MICOMM_PHY(x)         _DD_GETVALUE(x,S_MICOMM_PHY,M_MICOMM_PHY)

#define S_MICOMM_CMD            26
#define M_MICOMM_CMD            _DD_MAKEMASK(2,S_MICOMM_CMD)
#define V_MICOMM_CMD(x)         _DD_MAKEVALUE(x,S_MICOMM_CMD)
#define G_MICOMM_CMD(x)         _DD_GETVALUE(x,S_MICOMM_CMD,M_MICOMM_CMD)
#define K_MICOMM_CMD_WR         0x1
#define K_MICOMM_CMD_RD         0x2
#define V_MICOMM_CMD_WR         V_MICOMM_CMD(K_MICOMM_CMD_WR)
#define V_MICOMM_CMD_RD         V_MICOMM_CMD(K_MICOMM_CMD_RD)

#define M_MICOMM_RDFAIL         _DD_MAKEMASK1(28)
#define M_MICOMM_BUSY           _DD_MAKEMASK1(29)

/* MISTAT: MI Status Register (0x450) */

#define M_MISTAT_LINKED         _DD_MAKEMASK1(0)
#define M_MISTAT_10MBPS         _DD_MAKEMASK1(1)

/* MIMODE: MI Mode Register (0x454) */

#define M_MIMODE_SHORTPREAMBLE  _DD_MAKEMASK1(1)
#define M_MIMODE_POLLING        _DD_MAKEMASK1(4)

#define S_MIMODE_CLKCNT         16
#define M_MIMODE_CLKCNT         _DD_MAKEMASK(5,S_MIMODE_CLKCNT)
#define V_MIMODE_CLKCNT(x)      _DD_MAKEVALUE(x,S_MIMODE_CLKCNT)
#define G_MIMODE_CLKCNT(x)      _DD_GETVALUE(x,S_MIMODE_CLKCNT,M_MIMODE_CLKCNT)

/* TXLEN: Transmit MAC Lengths Register (0x464) */

#define S_TXLEN_SLOT            0
#define M_TXLEN_SLOT            _DD_MAKEMASK(8,S_TXLEN_SLOT)
#define V_TXLEN_SLOT(x)         _DD_MAKEVALUE(x,S_TXLEN_SLOT)
#define G_TXLEN_SLOT(x)         _DD_GETVALUE(x,S_TXLEN_SLOT,M_TXLEN_SLOT)

#define S_TXLEN_IPG             8
#define M_TXLEN_IPG             _DD_MAKEMASK(4,S_TXLEN_IPG)
#define V_TXLEN_IPG(x)          _DD_MAKEVALUE(x,S_TXLEN_IPG)
#define G_TXLEN_IPG(x)          _DD_GETVALUE(x,S_TXLEN_IPG,M_TXLEN_IPG)

#define S_TXLEN_IPGCRS         12
#define M_TXLEN_IPGCRS         _DD_MAKEMASK(2,S_TXLEN_IPGCRS)
#define V_TXLEN_IPGCRS(x)      _DD_MAKEVALUE(x,S_TXLEN_IPGCRS)
#define G_TXLEN_IPGCRS(x)      _DD_GETVALUE(x,S_TXLEN_IPGCRS,M_TXLEN_IPGCRS)

/* RULESCFG: Receive Rules Configuration Register (0x500) */

#define S_RULESCFG_DEFAULT     3
#define M_RULESCFG_DEFAULT     _DD_MAKEMASK(5,S_RULESCFG_DEFAULT)
#define V_RULESCFG_DEFAULT(x)  _DD_MAKEVALUE(x,S_RULESCFG_DEFAULT)
#define G_RULESCFG_DEFAULT(x)  _DD_GETVALUE(x,S_RULESCFG_DEFAULT,M_RULESCFG_DEFAULT)


/* Send Data Initiator Control Registers (p 383) */
/* Send BD Ring Selector Control Registers (p 387) */
/* Send BD Initiator Control Registers (p 389) */


/* Receive List Placement Control Registers (p 392) */

/* LISTCFG: Receive List Placement Configuration Register (0x2010) */

#define S_LISTCFG_GROUP        0
#define M_LISTCFG_GROUP        _DD_MAKEMASK(3,S_LISTCFG_GROUP)
#define V_LISTCFG_GROUP(x)     _DD_MAKEVALUE(x,S_LISTCFG_GROUP)
#define G_LISTCFG_GROUP(x)     _DD_GETVALUE(x,S_LISTCFG_GROUP,M_LISTCFG_GROUP)

#define S_LISTCFG_ACTIVE       3
#define M_LISTCFG_ACTIVE       _DD_MAKEMASK(5,S_LISTCFG_ACTIVE)
#define V_LISTCFG_ACTIVE(x)    _DD_MAKEVALUE(x,S_LISTCFG_ACTIVE)
#define G_LISTCFG_ACTIVE(x)    _DD_GETVALUE(x,S_LISTCFG_ACTIVE,M_LISTCFG_ACTIVE)

#define S_LISTCFG_BAD          8
#define M_LISTCFG_BAD          _DD_MAKEMASK(5,S_LISTCFG_BAD)
#define V_LISTCFG_BAD(x)       _DD_MAKEVALUE(x,S_LISTCFG_BAD)
#define G_LISTCFG_BAD(x)       _DD_GETVALUE(x,S_LISTCFG_BAD,M_LISTCFG_BAD)

#define S_LISTCFG_DEFAULT      13
#define M_LISTCFG_DEFAULT      _DD_MAKEMASK(2,S_LISTCFG_DEFAULT)
#define V_LISTCFG_DEFAULT(x)   _DD_MAKEVALUE(x,S_LISTCFG_DEFAULT)
#define G_LISTCFG_DEFAULT(x)   _DD_GETVALUE(x,S_LISTCFG_DEFAULT,M_LISTCFG_DEFAULT)


/* Receive Data and Receive BD Initiator Control Registers (p 399) */

/* RCVINITMODE: Receive Data and Receive BD Initiator Mode Register (0x2400) */

#define M_RCVINITMODE_JUMBO    _DD_MAKEMASK1(2)
#define M_RCVINITMODE_FRMSIZE  _DD_MAKEMASK1(3)
#define M_RCVINITMODE_RTNSIZE  _DD_MAKEMASK1(4)


/* Receive Initiator Control Registers (p 404) */
/* Receive BD Completion Control Registers (p 406) */
/* Receive List Selector Control Registers (p 408) */
/* Mbuf Cluster Free Registers (p 409) */


/* Host Coalescing Control registers (p 410) */

/* HCM: Host Coalescing Mode Register (0x3C00) */

#define M_HCM_RESET             _DD_MAKEMASK1(0)
#define M_HCM_ENABLE            _DD_MAKEMASK1(1)
#define M_HCM_ATTN              _DD_MAKEMASK1(2)
#define M_HCM_COAL_NOW          _DD_MAKEMASK1(3)

#define S_HCM_MSIBITS           4
#define M_HCM_MSIBITS           _DD_MAKEMASK(3,S_HCM_MSIBITS)
#define V_HCM_MSIBITS(x)        _DD_MAKEVALUE(x,S_HCM_MSIBITS)
#define G_HCM_MSIBITS           _DD_GETVALUE(x,S_HCM_MSIBITS,M_HCM_MSIBITS)

#define S_HCM_SBSIZE            7
#define M_HCM_SBSIZE            _DD_MAKEMASK(2,S_HCM_SBSIZE)
#define V_HCM_SBSIZE(x)         _DD_MAKEVALUE(x,S_HCM_SBSIZE)
#define G_HCM_SBSIZE            _DD_GETVALUE(x,S_HCM_SBSIZE,M_HCM_SBSIZE)
#define K_HCM_SBSIZE_80         0x0
#define K_HCM_SBSIZE_64         0x1
#define K_HCM_SBSIZE_32         0x2
/* ... more ... */


/* Memory Arbiter Registers (p 420) */

/* MAM: Memory Arbiter Mode Register (0x4000) */

#define M_MAM_RESET             _DD_MAKEMASK1(0)
#define M_MAM_ENABLE            _DD_MAKEMASK1(1)

/* Memory Arbiter Status Register (0x4004) */

/* Memory Arbiter Trap Low and Trap High Registers (0x4008, 0x400C) */


/* Buffer Manager Control Registers (p 424) */

/* BMODE: Buffer Manager Control Register (0x4400) */

#define M_BMODE_RESET           _DD_MAKEMASK1(0)
#define M_BMODE_ENABLE          _DD_MAKEMASK1(1)
#define M_BMODE_ATTN            _DD_MAKEMASK1(2)
#define M_BMODE_TEST            _DD_MAKEMASK1(3)
#define M_BMODE_MBUFLOWATTN     _DD_MAKEMASK1(4)


/* Read DMA Control Registers (p 428) */
/* Write DMA Control Registers (p 431) */

/* Bit fields shared by DMA_MODE and DMA_STATUS registers */

#define M_ATTN_TGTABORT         _DD_MAKEMASK1(2)
#define M_ATTN_MSTRABORT        _DD_MAKEMASK1(3)
#define M_ATTN_PERR             _DD_MAKEMASK1(4)
#define M_ATTN_ADDROVFL         _DD_MAKEMASK1(5)
#define M_ATTN_FIFOOVFL         _DD_MAKEMASK1(6)
#define M_ATTN_FIFOUNFL         _DD_MAKEMASK1(7)
#define M_ATTN_FIFOREAD         _DD_MAKEMASK1(8)
#define M_ATTN_LENERR           _DD_MAKEMASK1(9)
#define M_ATTN_ALL              (M_ATTN_TGTABORT | M_ATTN_MSTRABORT | \
                                 M_ATTN_PERR | M_ATTN_ADDROVFL | \
                                 M_ATTN_FIFOOVFL | M_ATTN_FIFOUNFL | \
                                 M_ATTN_FIFOREAD | M_ATTN_LENERR)

/* Read  DMA Mode Register (0x4800) */
/* Write DMA Mode Register (0x4C00) */

/* Read  DMA Status Register (0x4804) */
/* Write DMA Status Register (0x4C04) */


/* RX RISC Registers (p 433) */
/* TX RISC Registers (p 437) */
/* Low Priority Mailboxes (p 441) */
/* Flow Through Queues (p 445) */


/* Message Signaled Interrupt Registers (p 447) */

/* MSIMODE: MSI Mode Register (0x6000) */

#define M_MSIMODE_RESET         _DD_MAKEMASK1(0)
#define M_MSIMODE_ENABLE        _DD_MAKEMASK1(1)
#define M_MSIMODE_TGTABORT      _DD_MAKEMASK1(2)
#define M_MSIMODE_MSTRABORT     _DD_MAKEMASK1(3)
#define M_MSIMODE_PARITYERR     _DD_MAKEMASK1(4)
#define M_MSIMODE_FIFOUNRUN     _DD_MAKEMASK1(5)
#define M_MSIMODE_FIFOOVRUN     _DD_MAKEMASK1(6)

#define S_MSIMODE_PRIORITY      30
#define M_MSIMODE_PRIORITY      _DD_MAKEMASK(2,S_MSIMODE_PRIORITY)
#define V_MSIMODE_PRIORITY(x)   _DD_MAKEVALUE(x,S_MSIMODE_PRIORITY)
#define G_MSIMODE_PRIORITY(x)   _DD_GETVALUE(x,S_MSIMODE_PRIORITY,M_MSIMODE_PRIORITY)

/* MSISTAT: MSI Status Register (0x6004) */

#define M_MSISTAT_TGTABORT      _DD_MAKEMASK1(2)
#define M_MSISTAT_MSTRABORT     _DD_MAKEMASK1(3)
#define M_MSISTAT_PARITYERR     _DD_MAKEMASK1(4)
#define M_MSISTAT_FIFOUNRUN     _DD_MAKEMASK1(5)
#define M_MSISTAT_FIFOOVRUN     _DD_MAKEMASK1(6)

/* MSIDATA: MSI FIFO Access Register (0x6008) */

#define S_MSIFIFO_DATA           0
#define M_MSIFIFO_DATA           _DD_MAKEMASK(3,S_MSIFIFO_DATA)
#define V_MSIFIFO_DATA(x)        _DD_MAKEVALUE(x,S_MSIFIFO_DATA)
#define G_MSIFIFO_DATA(x)        _DD_GETVALUE(x,S_MSIFIFO_DATA,M_MSIFIFO_DATA)

#define M_MSIFIFO_OVFL          _DD_MAKEMASK1(3)


/* DMA Completion Registers (p 449) */


/* General Control registers (p 450) */

/* MCTL:  Miscellaneous Host Control Register (0x6800) */

#define M_MCTL_UPDATE           _DD_MAKEMASK1(0)
#define M_MCTL_BSWAPCTRL        _DD_MAKEMASK1(1)
#define M_MCTL_WSWAPCTRL        _DD_MAKEMASK1(2)
#define M_MCTL_BSWAPDATA        _DD_MAKEMASK1(4)
#define M_MCTL_WSWAPDATA        _DD_MAKEMASK1(5)
#define M_MCTL_NOCRACK          _DD_MAKEMASK1(9)
#define M_MCTL_NOCRC            _DD_MAKEMASK1(10)
#define M_MCTL_ACCEPTBAD        _DD_MAKEMASK1(11)
#define M_MCTL_NOTXINT          _DD_MAKEMASK1(13)
#define M_MCTL_NORTRNINT        _DD_MAKEMASK1(14)
#define M_MCTL_PCI32            _DD_MAKEMASK1(15)
#define M_MCTL_HOSTUP           _DD_MAKEMASK1(16)
#define M_MCTL_HOSTBDS          _DD_MAKEMASK1(17)
#define M_MCTL_NOTXPHSUM        _DD_MAKEMASK1(20)
#define M_MCTL_NORXPHSUM        _DD_MAKEMASK1(23)
#define M_MCTL_TXINT            _DD_MAKEMASK1(24)
#define M_MCTL_RXINT            _DD_MAKEMASK1(25)
#define M_MCTL_MACINT           _DD_MAKEMASK1(26)
#define M_MCTL_DMAINT           _DD_MAKEMASK1(27)
#define M_MCTL_FLOWINT          _DD_MAKEMASK1(28)
#define M_MCTL_4XRINGS          _DD_MAKEMASK1(29)
#define M_MCTL_MCASTEN          _DD_MAKEMASK1(30)

/* MCFG:  Miscellaneous Configuration Register (0x6804) */

#define M_MCFG_CORERESET        _DD_MAKEMASK1(0)
#define S_MCFG_PRESCALER        1
#define M_MCFG_PRESCALER        _DD_MAKEMASK(7,S_MCFG_PRESCALER)
#define V_MCFG_PRESCALER(x)     _DD_MAKEVALUE(x,S_MCFG_PRESCALER)
#define G_MCFG_PRESCALER(x)     _DD_GETVALUE(x,S_MCFG_PRESCALER,M_MCFG_PRESCALER)

/* MLCTL: Miscellaneous Local Control Register (0x6808) */

#define M_MLCTL_INTSTATE        _DD_MAKEMASK1(0)
#define M_MLCTL_INTCLR          _DD_MAKEMASK1(1)
#define M_MLCTL_INTSET          _DD_MAKEMASK1(2)
#define M_MLCTL_INTATTN         _DD_MAKEMASK1(3)
/* ... */
#define M_MLCTL_EPAUTOACCESS    _DD_MAKEMASK1(24)

/* EPADDR: Serial EEPROM Address Register (0x6838) */

#define S_EPADDR_ADDR            0
#define M_EPADDR_ADDR            (_DD_MAKEMASK(16,S_EPADDR_ADDR) & ~3)
#define V_EPADDR_ADDR(x)         _DD_MAKEVALUE(x,S_EPADDR_ADDR)
#define G_EPADDR_ADDR(x)         _DD_GETVALUE(x,S_EPADDR_ADDR,M_EPADDR_ADDR)

#define S_EPADDR_HPERIOD         16
#define M_EPADDR_HPERIOD         _DD_MAKEMASK(9,S_EPADDR_HPERIOD)
#define V_EPADDR_HPERIOD(x)      _DD_MAKEVALUE(x,S_EPADDR_HPERIOD)
#define G_EPADDR_HPERIOD(x)      _DD_GETVALUE(x,S_EPADDR_HPERIOD,M_EPADDR_HPERIOD)

#define M_EPADDR_START           _DD_MAKEMASK1(25)

#define S_EPADDR_DEVID           26
#define M_EPADDR_DEVID           _DD_MAKEMASK(3,S_EPADDR_DEVID)
#define V_EPADDR_DEVID(x)        _DD_MAKEVALUE(x,S_EPADDR_DEVID)
#define G_EPADDR_DEVID(x)        _DD_GETVALUE(x,S_EPADDR_DEVID,M_EPADDR_DEVID)

#define M_EPADDR_RESET           _DD_MAKEMASK1(29)
#define M_EPADDR_COMPLETE        _DD_MAKEMASK1(30)
#define M_EPADDR_RW              _DD_MAKEMASK1(31)

/* EPDATA: Serial EEPROM Data Register (0x683C) */

/* EPCTL: Serial EEPROM Control Register (0x6840) */

#define M_EPCTL_CLOCKTS0         _DD_MAKEMASK1(0)
#define M_EPCTL_CLOCKO           _DD_MAKEMASK1(1)
#define M_EPCTL_CLOCKI           _DD_MAKEMASK1(2)
#define M_EPCTL_DATATSO          _DD_MAKEMASK1(3)
#define M_EPCTL_DATAO            _DD_MAKEMASK1(4)
#define M_EPCTL_DATAI            _DD_MAKEMASK1(5)

/* MDCTL: MDI Control Register (0x6844) */

#define M_MDCTL_DATA             _DD_MAKEMASK1(0)
#define M_MDCTL_ENABLE           _DD_MAKEMASK1(1)
#define M_MDCTL_SELECT           _DD_MAKEMASK1(2)
#define M_MDCTL_CLOCK            _DD_MAKEMASK1(3)


/* Ring Control Blocks (p 97) */

#define RCB_HOST_ADDR_HIGH       0x0
#define RCB_HOST_ADDR_LOW        0x4
#define RCB_CTRL                 0x8
#define RCB_NIC_ADDR             0xC

#define RCB_SIZE                 0x10

#define RCB_FLAG_USE_EXT_RCV_BD  _DD_MAKEMASK1(0)
#define RCB_FLAG_RING_DISABLED   _DD_MAKEMASK1(1)

#define S_RCB_MAXLEN             16
#define M_RCB_MAXLEN             _DD_MAKEMASK(16,S_RCB_MAXLEN)
#define V_RCB_MAXLEN(x)          _DD_MAKEVALUE(x,S_RCB_MAXLEN)
#define G_RCB_MAXLEN(x)          _DD_GETVALUE(x,S_RCB_MAXLEN,M_RCB_MAXLEN)


/* On-chip Memory Map (Tables 70 and 71, pp 178-179) This is the map
   for the 5700 with no external SRAM, the 5701, 5702 and 5703.  The
   5705 does not fully implement some ranges and maps the buffer pool
   differently. */

/* Locations  0x0000 - 0x00FF are Page Zero */

/* Locations  0x0100 - 0x01FF are Send Producer Ring RCBs */

#define A_SND_RCBS              0x0100
#define L_SND_RCBS              (16*RCB_SIZE)
#define A_SND_RCB(n)            (A_SND_RCBS + ((n)-1)*RCB_SIZE)

/* Locations  0x0200 - 0x02FF are Receive Return Ring RCBs */

#define A_RTN_RCBS              0x0200
#define L_RTN_RCBS              (16*RCB_SIZE)
#define A_RTN_RCB(n)            (A_RTN_RCBS + ((n)-1)*RCB_SIZE)

/* Locations  0x0300 - 0x0AFF are Statistics Block */

#define A_MAC_STATS             0x0300
#define L_MAC_STATS             (0x0B00-A_MAC_STATS)

/* Locations  0x0B00 - 0x0B4F are Status Block */

#define A_MAC_STATUS            0x0B00
#define L_MAC_STATUS            (0x0B50-A_MAC_STATUS)

/* Locations  0x0B50 - 0x0FFF are Software General Communication */

#define A_PXE_MAILBOX           0x0B50
#define T3_MAGIC_NUMBER         0x4B657654

/* Locations  0x1000 - 0x1FFF are unmapped */

/* Locations  0x2000 - 0x3FFF are DMA Descriptors */

#define A_DMA_DESCS             0x2000
#define L_DMA_DESCS             (0x4000-A_DMA_DESCS)

/* Locations  0x4000 - 0x5FFF are Send Rings 1-4 */

#define A_SND_RINGS             0x4000
#define L_SND_RINGS             (0x6000-A_SND_RINGS)

/* Locations  0x6000 - 0x6FFF are Standard Receive Rings */

#define A_STD_RCV_RINGS         0x6000
#define L_STD_RCV_RINGS         (0x7000-A_STD_RCV_RINGS)

/* Locations  0x7000 - 0x7FFF are Jumbo Receive Rings */

#define A_JUMBO_RCV_RINGS       0x7000
#define L_JUMBO_RCV_RINGS       (0x8000-A_JUMBO_RCV_RINGS)

/* Locations  0x08000 - 0x0FFFF are Buffer Pool 1 */
/* Locations  0x10000 - 0x17FFF are Buffer Pool 2 */
/* Locations  0x18000 - 0x1FFFF are Buffer Pool 3 */

#define A_BUFFER_POOL           0x08000
#define L_BUFFER_POOL           (0x20000-A_BUFFER_POOL)

/* Locations  0x08000 - 0x09FFF are TXMBUF (5705) */
/* Locations  0x10000 - 0x1DFFF are RXMBUF (5705) */

#define A_TXMBUF                0x08000
#define L_TXMBUF                (0x0A000-A_TXMBUF)
#define A_RXMBUF                0x10000
#define L_RXMBUF                (0x1E000-A_RXMBUF)


/* Indices of (8-byte) counters in the Statistics Block. */

#define ifHCInOctets                         32
#define etherStatsFragments                  34
#define ifHCInUcastPkts                      35
#define ifHCInMulticastPkts                  36
#define ifHCInBroadcastPkts                  37
#define dot3StatsFCSErrors                   38
#define dot3StatsAlignmentErrors             39
#define xonPauseFramesReceived               40
#define xoffPauseFramesReceived              41
#define macControlFramesReceived             42
#define xoffSateEntered                      43
#define dot3StatsFrameTooLongs               44
#define etherStatsJabbers                    45
#define etherStatsUndersizePkts              46
#define inRangeLengthError                   47
#define outRangeLengthError                  48
#define etherStatsPkts64Octets               49
#define etherStatsPkts65to127Octets          50
#define etherStatsPkts128to255Octets         51
#define etherStatsPkts256to511Octets         52
#define etherStatsPkts512to1023Octets        53
#define etherStatsPkts1024to1522Octets       54
#define etherStatsPkts1523to2047Octets       55
#define etherStatsPkts2048to4095Octets       56
#define etherStatsPkts4096to8191Octets       57
#define etherStatsPkts8192to9022Octets       58

#define ifHCOutOctets                        96
#define etherStatsCollisions                 98
#define outXonSent                           99
#define outXoffSent                         100
#define flowControlDone                     101
#define dot3StatsInternalMacTransmitErrors  102
#define dot3StatsSingleCollisionFrames      103
#define dot3StatsMultipleCollisionFrames    104
#define dot3StatsDeferredTransmissions      105
#define dot3StatsExcessiveCollisions        107
#define dot3StatsLateCollisions             108
#define dot3Collided2Times                  109
#define dot3Collided3Times                  110
#define dot3Collided4Times                  111
#define dot3Collided5Times                  112
#define dot3Collided6Times                  113
#define dot3Collided7Times                  114
#define dot3Collided8Times                  115
#define dot3Collided9Times                  116
#define dot3Collided10Times                 117
#define dot3Collided11Times                 118
#define dot3Collided12Times                 119
#define dot3Collided13Times                 120
#define dot3Collided14Times                 121
#define dot3Collided15Times                 122
#define ifHCOutUcastPkts                    123
#define ifHCOutMulticastPkts                124
#define ifHCOutBroadcastPkts                125
#define dot3StatsCarrierSenseErrors         126
#define ifOutDiscards                       127
#define ifOutErrors                         128

#define COSifHCInPkts1                      160
#define COSifHCInPkts2                      161                
#define COSifHCInPkts3                      162
#define COSifHCInPkts4                      163
#define COSifHCInPkts5                      164
#define COSifHCInPkts6                      165
#define COSifHCInPkts7                      166
#define COSifHCInPkts8                      167
#define COSifHCInPkts9                      168
#define COSifHCInPkts10                     169
#define COSifHCInPkts11                     170
#define COSifHCInPkts12                     171
#define COSifHCInPkts13                     172
#define COSifHCInPkts14                     173
#define COSifHCInPkts15                     174
#define COSifHCInPkts16                     175
#define COSFramesDroppedDueToFilters        176
#define nicDmaWriteQueueFull                177
#define nicDmaWriteHighPriQueueFull         178
#define nicNoMoreRxBDs                      179
#define ifInDiscards                        180
#define ifInErrors                          181
#define nicRecvThresholdHit                 182

#define COSifHCOutPkts1                     192
#define COSifHCOutPkts2                     193
#define COSifHCOutPkts3                     194
#define COSifHCOutPkts4                     195
#define COSifHCOutPkts5                     196
#define COSifHCOutPkts6                     197
#define COSifHCOutPkts7                     198
#define COSifHCOutPkts8                     199
#define COSifHCOutPkts9                     200
#define COSifHCOutPkts10                    201
#define COSifHCOutPkts11                    202
#define COSifHCOutPkts12                    203
#define COSifHCOutPkts13                    204
#define COSifHCOutPkts14                    205
#define COSifHCOutPkts15                    206
#define COSifHCOutPkts16                    207
#define nicDmaReadQueueFull                 208
#define nicDmaReadHighPriQueueFull          209
#define nicSendDataCompQueueFull            210
#define nicRingSetSendProdIndex             211
#define nicRingStatusUpdate                 212
#define nicInterrupts                       213
#define nicAvoidedInterrupts                214
#define nicSendThresholdHit                 215

#endif /* _BCM_5700_H_ */
