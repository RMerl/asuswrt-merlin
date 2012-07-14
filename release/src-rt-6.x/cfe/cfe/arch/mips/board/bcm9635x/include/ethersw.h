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
/*
** ethersw.h   Register offsets, bit definitions and structures
**              for programming the 3-ports ethernet swith
**
**
** Revision 1.0 2001/05/29 GT
** add register definition for MII 
**
** $Id: ethersw.h 241205 2011-02-17 21:57:41Z $ 
**
**
*/
#ifndef BCMTYPES_H
#include <bcmtypes.h>       //seanl. 
#endif

//*******************************************************
//
//      Register/Port Definitions for ethersw
//
//
//  Definition Legend:
//   _dp: page address decoding
//   _d:  offset address decoding
//   _b:  bit position
//   _a:  register/port address allocation (retired for most of pages)
//   _w:  register/port width or length
//   _s:  register/port input wire instantiation
//   _g:  register/port instantiation 
//
//  Ways to use register/port during coding:
//      1) In the module that needs to use one(or multiple) reg/port, define page register(s)
//         i/o bundle wiring(s) as input/output, be sure to use pre-defined names for bundle 
//         wirings and never miss or delete and wire bits with a bundle wiring; 
//      2) Include this file after module port list but before any declared port;
//      3) After port declaration, declare any page instantiation define needed to be used in this 
//         module, using defined (page) bundle name(s) with surfix _inatance;
//      4) now a register with _g surfix can be used;
//      5) Define any bit name(s) after that if this file has pre-defined any of those, or 
//         create any other name(s) alias;
//      6) Synthesis tool must remove all the unused wiring bundle bits declared as ports and
//         all unsed reg bit DFFs to reduce actual used wires and save gates. 
//      7) Example - coding of box_id[1:0] and dev_id[1:0]; values that need to be sent to
//         registers 
//
//
//*******************************************************






// -------------------------------------------------------------
// the following is the definitions of registers for BCM6352
// -------------------------------------------------------------

// registers page address
#define  CTLREG_dp      0x0000
#define  STSREG_dp      0x0100
#define  MNGMODE_dp     0x0200
#define  MIBACST_dp     0x0300
#define  ARLCTL_dp      0x0400
#define  ARLACCS_dp     0x0500
#define  SMPACCS_dp     0x0600
#define  MIC5308_dp     0x0700
#define  MEM_dp         0x0800
#define  DIAGREG_dp     0x0900          // diagnostic regsters
#define  CNGMREG_dp     0x0a00          // Congestion management
#define  MIBDIAG_dp     0x0b00          // MIB diagnostic
#define  PORT0_MII_dp   0x0c00
#define  PORT1_MII_dp   0x0d00
#define  PORT0_MIB_dp   0x0e00
#define  PORT1_MIB_dp   0x0f00

#define  SPICTL_d       0xfd               // SPI Control Register
#define  SPISTS_d       0xfe               // SPI Status Register
        // [7] (SPIF): SPI R/W Complete Flag(self-clearing? no!)
        // [5] (RACK): SPI read data ready ack(self-clearing)
        // [1] (TXRDY): SMP Tx Ready Flag - should check it every 8 bytes
        // [0] (RXRDY): SMP Rx Ready Flag - should check it every 8 bytes
        #define SPIF_b  7
        #define RACK_b  5
        #define TXRDY_b 1
        #define RXRDY_b 0
#define  PAGEREG_d      0xff             

// Control Registers

#define  TH_PCTL0_d     0x00
        #define RX_DIS_b        0
        #define TX_DIS_b        1
        #define TH_PCTL_RSRV0_b 2
        #define STP_STATE_b     5
#define  TH_PCTL1_d     0x04
#define  TH_PCTL2_d     0x08
#define  TH_PCTL3_d     0x0c
#define  TH_PCTL4_d     0x10
#define  TH_PCTL5_d     0x14
#define  TH_PCTL6_d     0x18
#define  TH_PCTL7_d     0x1c
#define  MII_PCTL_d     0x20
        #define MIRX_DIS_b      0
        #define MITX_DIS_b      1
        #define MIRX_BC_DIS_b   2
        #define MIRX_MC_DIS_b   3
        #define MIRX_UC_DIS_b   4
        #define MISTP_STATE_b   5
#define  EXP_PCTL_d     0x24
        #define EXP_PCTL_RSRV0_b 0
#define  SMP_CTL_d      0x28
        #define SMP_CTL_RSRV0_b  0
        #define RX_MCST_DIS_b    2
        #define RX_BCST_DIS_b    3
        #define RX_UCST_DIS_b    4
#define  SWMODE_d       0x2c
        #define SW_FWDG_MODE_b   0
        #define SW_FWDG_EN_b     1
        #define RTRY_LMT_DIS_b   2
        #define SWMODE_RSRV0_b   3
#define  PAUSEQT_d      0x30
        #define PAUSEQT_b        0
        #define PAUSEQT_RSRV0_b  15
#define  MII_PSTS_d     0x34
        #define MII_LINK_b       0
        #define MII_SPD_b        1
        #define MII_FDX_b        2
        #define MII_FCAP_b       3
        #define MII_OVRD_b       7
#define  LED_FLSH_CTL_d 0x38
        #define LED_BLINK_b              0 
        #define LED_FLSH_CTL_RSRV0_b     8
#define  LEDA_ST_d      0x3c
        #define P0A_ST_b         0
        #define P1A_ST_b         2
        #define P2A_ST_b         4
        #define P3A_ST_b         6
        #define P4A_ST_b         8
        #define P5A_ST_b         10
        #define P6A_ST_b         12
        #define P7A_ST_b         14
#define  LEDB_ST_d      0x40
        #define P0B_ST_b         0
        #define P1B_ST_b         2
        #define P2B_ST_b         4
        #define P3B_ST_b         6
        #define P4B_ST_b         8
        #define P5B_ST_b         10
        #define P6B_ST_b         12
        #define P7B_ST_b         14
#define  LEDC_ST_d      0x44
        #define P0C_ST_b         0
        #define P1C_ST_b         2
        #define P2C_ST_b         4
        #define P3C_ST_b         6
        #define P4C_ST_b         8
        #define P5C_ST_b         10
        #define P6C_ST_b         12
        #define P7C_ST_b         14
#define  MISC_LED_ST_d  0x48
        #define MISC_LED_RSRV0_b 0
        #define MII_A_ST_b       8
        #define MII_B_ST_b       10
        #define MII_C_ST_b       12
        #define ERROR_ST_b       14
#define  PRI_CTL0_d     0x4c
#define  PRI_CTL1_d     0x50
#define  CPU_PRI_CTL_d  0x54
#define  PRI_MAP0_d     0x58
#define  PRI_MAP1_d     0x5c
#define  CPU_PRI_MAP_d  0x60

// Status Registers

#define LNKSTS_d        0x00
#define LNKSTSCHG_d     0x04
#define SPDSTS_d        0x08
#define DUPSTS_d        0x0c
#define PAUSESTS_d      0x10
#define CNGSSTS_d       0x14
#define SRCADRCHG_d     0x18
#define LSA_PORT0_d     0x20
#define LSA_PORT1_d     0x28
#define LSA_PORT2_d     0x30
#define LSA_PORT3_d     0x38
#define LSA_PORT4_d     0x40
#define LSA_PORT5_d     0x48
#define LSA_PORT6_d     0x50
#define LSA_PORT7_d     0x58
#define LSA_MIIPORT_d   0x60
#define BIST_STS_d      0x68

// Management Mode Registers

#define  GMNGCFG_d      0x00
        #define RST_MIB_CNTRS_b         0
        #define RX_BPDU_b               1
        #define GMNGCFG_dRSRV0_b        2
        #define MAH_CTL_b               4
        #define MACST_EN_b              5
        #define PHY_MNGP_b              6
#define  CHPBOXID_d     0x04
        #define CHPBOXID_RSRV0_b        0
        #define CHIPID_b                4
        #define BOXID_b                 6
#define  MNGPID_d       0x08
        #define MNGPID_b                0
        #define MNGCID_b                4
        #define MNGBID_b                6
#define  RMONSTEER_d    0x0c
        #define OR_RMON_RCV_b           0
        #define RMNSTR_RSRV0_b          9
#define  SPTAGT_d       0x10
        #define AGE_TIME_b              0
        #define SPTAGT_RSRV0_b          20
#define  MIRCAPCTL_d    0x14
        #define MIR_CAP_PORT_b          0
        #define MIRCAPCTL_RSRV0_b       11
        #define MIR_EN_b                15
#define  IGMIRCTL_d     0x18
        #define IN_MIR_MSK_b            0
        #define IGMIRCTL_RSRV0_b        11
        #define IN_DIV_EN_b             13
        #define IN_MIR_FLTR_b           14
#define  IGMIRDIV_d     0x1c
        #define IN_MIR_DIV_b            0
        #define IGMIRDIV_RSRV0_b        10
#define  IGMIRMAC_d     0x20
        #define IN_MIR_MAC_b            0
#define  EGMIRCTL_d     0x28
        #define OUT_MIR_MSK_b           0
        #define EGMIRCTL_RSRV0_b        11
        #define OUT_DIV_EN_b            13
        #define OUT_MIR_FLTR_b          14
#define  EGMIRDIV_d     0x2c
        #define OUT_MIR_DIV_b           0
        #define EGMIRDIV_RSRV0_b        10
#define  EGMIRMAC_d     0x30
        #define OUT_MIR_MAC_b           0

// MIB Autocast Registers

#define  ACSTPORT_d     0x00
        #define MIB_AC_PORTS_b   0
        #define ACSTPORT_RSRV0_b         11
#define  ACSTHDPT_d     0x04
        #define MIB_AC_HDR_b             0
#define  ACSTHDLT_d     0x08
        #define MIB_AC_HDR_LEN_b         0
        #define ACSTHDLT_RSRV0_b         8
#define  ACSTDA_d       0x10
        #define MIB_AC_DA_b              0
#define  ACSTSA_d       0x18
        #define MIB_AC_SA_b              0
#define  ACSTTYPE_d     0x20
        #define MIB_AC_TYPE_b            0
           #define BRCM_ET_TYPE_df       0
#define  ACSTRATE_d     0x24
        #define MIB_AC_RATE_b            0
        #define MIB_AC_RATE_df           64

// ARL Control Registers

#define  GARLCFG_d      0x00
        #define HASH_DISABLE_b          0
        #define SINGLERW_EN_b           1
        #define GARLCFG_RSRV0_b         2
#define  BPDU_MCADDR_d  0x08
        #define BPDU_MC_ADDR_b          0
#define  GRPADDR1_d     0x10
        #define GRP_ADDR_b              0
#define  PORTVEC1_d     0x18
        #define PORT_VCTR_b             0
        #define PORTVEC_RSRV0_b         11
#define  GRPADDR2_d     0x20
#define  PORTVEC2_d     0x28
#define  SEC_SPMSK_d    0x2C
#define  SEC_DPMSK_d    0x30


/* 
** ARL Access registers - memory page 0x05
*/
#define ARLA_RWCTL_d    0x00     /* ARL Read/Write Control register */
        #define ARL_RW_b                      0
        #define ARLA_RWCTL_RSRV0_b      1
        #define ARL_STRTDN_b               7
#define ARLA_MAC_d      0x08        /* ARL MAC Address Index register */
        #define MAC_ADDR_INDX_b         0
#define ARLA_ENTRY0_d   0x10     /* ARL Entry 0 and 1 register */       
#define ARLA_ENTRY1_d   0x20     /* ARL Entry 0 and 1 register */       
        #define ARL_MACADDR_b           0
        #define ARL_PID_b                     48
        #define ARL_CID_b                     52
        #define ARL_BID_b                     54
        #define ARLA_ENTRY_RSRV0_b      56
        #define ARL_AGE_b                     61
        #define ARL_STATIC_b               62
        #define ARL_VALID_b                63
#define ARLA_SRCH_CTL_d 0x30     /* ARL Search Control register */      
#define ARLA_SRCH_ADR_d 0x34     /* ARL Search Address register */      
#define ARLA_SRCH_RSLT_d 0x38    /* ARL Search Result register */       

// SMP Access Registers

#define  SMPIG_PORT_d   0x00
#define  SMPEG_PORT_d   0x04
#define  SMPEG_CTL_d    0x08
        #define SMP_SOF_b               0
        #define SMP_EOF_b               1
        #define SMP_RXRDY_b             2
#define  SMPIG_STS_d    0x10
        #define SMP_NEW_FRM_b           0
        #define SMP_FRM_PRO_b           1
        #define SMP_TXRDY_b             2
        #define SMP_FRM_SIZE_b          5

// MIC5308 Access Registers

#define MIC_PORT0_d     0x00
#define MIC_PORT1_d     0x02
#define MIC_PORT2_d     0x04
#define MIC_PORT3_d     0x06
#define MIC_PORT4_d     0x08
#define MIC_PORT5_d     0x0a
#define MIC_PORT6_d     0x0c
#define MIC_PORT7_d     0x0e
#define MIC_PORT8_d     0x10
#define MIC_PORT9_d     0x12
#define MIC_PORTa_d     0x14
#define MIC_PORTb_d     0x16
#define MIC_PORTc_d     0x18
#define MIC_PORTd_d     0x1a
#define MIC_PORTe_d     0x1c
#define MIC_PORTf_d     0x1e
#define MIC_RSRV_d      0x20

// MEM Access Registers

#define  MEMADR_d       0x00
        #define MEM_ADR_b               0
        #define MEM_RW_b                18
        #define MEM_STDN_b              19
#define  MEMDAT_d       0x08
#define MIBKILLOVR_d    0x2d

/* 
** Port MII registers - memory page 0x0c~0x0d
*/
#define  MIICTL_d       0x00  /* MII Control register */
   #define MII_RESET_b    0x8000   /* reset */
   #define MII_LPBK_b     0x4000   /* loopback */
   #define MII_FORCE100_b 0x2000   /* force 100Mbs */
   #define MII_AUTONEG_b  0x1000   /* auto-negotiation */
   #define MII_FDEPLEX_b  0x0100   /* full-duplex */
#define  MIISTS_d       0x02  /* MII Status register */         
#define  PHYIDH_d       0x04  /* PHY ID high */
#define  PHYIDL_d       0x06  /* PHY ID low */            
#define  ANADV_d        0x08  /* Auto-Negotiation Advertisement */             
#define  ANLPA_d        0x0a  /* Auto-Negotiation Link Partner Ability */                          
#define  ANEXP_d        0x0c  /* Auto-Negotiation Expansion */                          
#define  MII_RSRV0_d    0x0e           
#define  HNDRD_ACTL_d   0x20          
#define  HNDRD_ASTS_d   0x22          
#define  HNDRD_RECNT_d  0x24                 
#define  HNDRD_FCSCNT_d 0x26                
#define  MII_RSRV1_d    0x28           
#define  CHIP_1100ID (CHIPID & 0x000000FF)
#if (CHIP_1100ID < 0xB0)
   #define  MII_RSRV4_d    0x2C    /* Bit 1 = Gamma 1/8 bit */
   #define  MII_GAMMA_b    0x0002  /* gamma 1/8 bit enabled */
#endif   
#define  ACTLSTS_d      0x30             
#define  ASTSSUM_d      0x32             
#define  MII_RSRV2_d    0x34           
#define  AMODE2_d       0x36              
#define  AEGSTS_d       0x38              
#define  MII_RSRV3_d    0x3a           
#define  AMPHY_d        0x3c               
#define  BRCMTST_d      0x3e             
#define  MII_RSRV_d     0x40           

// MIB registers 

#define  TxOctets_d             0x00            // 64-bit              
#define  TxDropPkts_d           0x08          
#define  TxGoodPkts_d           0x0c          
#define  MIB_Hole0_d            0x0c           
#define  TxBroadcastPkts_d      0x10             
#define  TxMulticastPkts_d      0x14             
#define  TxUnicastPkts_d        0x18               
#define  TxCollisions_d         0x1c                
#define  TxSingleCollision_d    0x20           
#define  TxMultipleCollision_d  0x24                 
#define  TxDeferredTransmit_d   0x28          
#define  TxLateCollision_d      0x2c             
#define  TxExcessiveCollision_d 0x30                
#define  TxFrameInDisc_d        0x34               
#define  TxPausePkts_d          0x38                 
#define  TxReserved2_d          0x3c               
#define  TxReserved3_d          0x40               
#define  RxOctets_d             0x44            // 64-bit               
#define  RxUndersizePkts_d      0x4c             
#define  RxPausePkts_d          0x50                 
#define  Pkts64Octets_d         0x54                
#define  Pkts65to127Octets_d    0x58           
#define  Pkts128to255Octets_d   0x5c          
#define  Pkts256to511Octets_d   0x60          
#define  Pkts512to1023Octets_d  0x64                 
#define  Pkts1024to1522Octets_d 0x68                
#define  RxOversizePkts_d       0x6c              
#define  RxJabbers_d            0x70             
#define  RxAlignmentErrors_d    0x74           
#define  RxFCSErrors_d          0x78                 
#define  RxGoodOctets_d         0x7c        // 64-bit               
#define  RxDropPkts_d           0x84          
#define  RxUnicastPkts_d        0x88               
#define  RxMulticastPkts_d      0x8c             
#define  RxBroadcastPkt_d       0x90              
#define  RxSAChanges_d          0x94              
#define  RxFragments_d          0x98                 
#define  RxExcessSizeDisc_d     0x9c            
#define  RXSymblErr_d           0xA0            
#define  MIB_RSRV_d             0xA4           

// Diagnostic registers
// --- ARL

#define PORT0_RAWPORTMAP_d       0x00
#define PORT0_FINALPORTMAP_d     0x02
#define PORT1_RAWPORTMAP_d       0x04
#define PORT1_FINALPORTMAP_d     0x06
#define PORT2_RAWPORTMAP_d       0x08
#define PORT2_FINALPORTMAP_d     0x0a
#define PORT3_RAWPORTMAP_d       0x0c
#define PORT3_FINALPORTMAP_d     0x0e
#define PORT4_RAWPORTMAP_d       0x10
#define PORT4_FINALPORTMAP_d     0x12
#define PORT5_RAWPORTMAP_d       0x14
#define PORT5_FINALPORTMAP_d     0x16
#define PORT6_RAWPORTMAP_d       0x18
#define PORT6_FINALPORTMAP_d     0x1a
#define PORT7_RAWPORTMAP_d       0x1c
#define PORT7_FINALPORTMAP_d     0x1e
#define PORT8_RAWPORTMAP_d       0x20
#define PORT8_FINALPORTMAP_d     0x22
#define PORT9_RAWPORTMAP_d       0x24
#define PORT9_FINALPORTMAP_d     0x26
#define PORT10_RAWPORTMAP_d      0x28
#define PORT10_FINALPORTMAP_d    0x2a

#define DIAG_RSRV1_d             0x2c


#define BUFCON_BUFCONREQ0_d    0xd0
#define BUFCON_BUFCONREQ1_d    0xd2
#define BUFCON_BUFCONREQ2_d    0xd4
#define BUFCON_BUFCONREQ3_d    0xd6
#define BUFCON_BUFCONREQ4_d    0xd8
#define BUFCON_BUFCONREQ5_d    0xda
#define BUFCON_BUFCONREQ6_d    0xdc
#define BUFCON_BUFCONREQ7_d    0xde
#define BUFCON_BUFCONREQ8_d    0xe0
#define BUFCON_BUFCONREQ9_d    0xe2
#define BUFCON_BUFCONREQ10_d   0xe4
#define BUFCON_BUFCONREQ11_d   0xe6
#define BUFCON_AVAIL_BUF0_d    0xe8
#define BUFCON_AVAIL_BUF1_d    0xea
#define BUFCON_AVAIL_BUF2_d    0xec
#define BUFCON_AVAIL_BUF3_d    0xee

#define DIAG_RSRV2_d             0xf0

// --- Flow Control
// This is the address MAP for registers in flowcon.
// it is not used by the logic but is used by the
// test bench 

#define FCON_DIAG_FLOW_d     0x00
#define FCON_10_TH_HYST_d    0x02 
// FCON_10_TH_HYST_d and FCON_10_TH_PAUSE_d
#define FCON_10_TH_DROP_d    0x04
#define FCON_100_TH_HYST_d   0x06 
// FCON_100_TH_HYST_d and FCON_100_TH_PAUSE_d
#define FCON_100_TH_DROP_d   0x08
#define FCON_EXP_TH_HYST_d   0x0a 
// FCON_EXP_TH_HYST_d and FCON_EXP_TH_PAUSE_d
#define FCON_EXP_TH_DROP_d   0x0c
#define FCON_TOT_TH_HYST_d   0x0e
// FCON_TOT_TH_HYST_d and FCON_TOT_TH_PAUSE_d 
#define FCON_TOT_TH_DROP_d   0x10
#define FCON_TXQ_FULL_TH_d   0x12
#define RSRV_FLOWCON0_d       0x14 
#define FCON_CONG_MAP0_d      0x20
#define FCON_CONG_MAP1_d      0x22
#define FCON_CONG_MAP2_d      0x24
#define FCON_DROP_HIST_d      0x26
#define FCON_PAUSE_HIST_d     0x28
#define FCON_PEAK_TOTAL_USED_d  0x2a
#define FCON_PEAK_TXQ_CNT_d     0x2c
#define FCON_PEAK_RXBCNT_d      0x2e
#define FCON_FLOWMIX_d          0x30
// Not useful now - hchoy
#define RSRV_FCONCON1_d         0x32
#define DIAG_RSRV_d           0x62
