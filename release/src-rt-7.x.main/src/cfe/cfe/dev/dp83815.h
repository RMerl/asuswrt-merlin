/*
 * Register and bit definitions for the National Semiconductor
 *   DP83815 10/100 Integrated Ethernet MAC and PHY.
 * Reference:
 *   DP83815 10/100 Mb/s Integrated PCI Ethernet Media Access
 *     Controller and Physical Layer (MacPhyter)
 *     Hardware Reference Manual, Revision 1.0.
 *   National Semiconductor Corp., December 2000
 */
#ifndef _DP83815_H_
#define _DP83815_H_

#define _DD_MAKEMASK1(n) (1 << (n))
#define _DD_MAKEMASK(v,n) ((((1)<<(v))-1) << (n))
#define _DD_MAKEVALUE(v,n) ((v) << (n))
#define _DD_GETVALUE(v,n,m) (((v) & (m)) >> (n))


/*  PCI Configuration Register offsets (MacPhyter nomenclature) */

#define R_CFGID          PCI_ID_REG
#define K_PCI_VENDOR_NSC 0x100B
#define K_PCI_ID_DP83815 0x0020

#define R_CFGCS          PCI_COMMAND_STATUS_REG
#define R_CFGRID         PCI_CLASS_REG
#define R_CFGLAT         PCI_BHLC_REG

#define R_CFGIOA         PCI_MAPREG(0)
#define R_CFGMA          PCI_MAPREG(1)

#define R_CFGSID         PCI_SUBSYS_ID_REG
#define R_CFGROM         PCI_MAPREG_ROM
#define R_CAPPTR         PCI_CAPLISTPTR_REG
#define R_CFGINT         PCI_BPARAM_INTERRUPT_REG

/* Power management capability */
#define R_PMCAP          0x40
#define R_PMCSR          0x44


/*  MacPhyter Operational Register offsets */
    
#define R_CR                  0x00
#define R_CFG                 0x04
#define R_MEAR                0x08
#define R_PTSCR               0x0C
#define R_ISR                 0x10
#define R_IMR                 0x14
#define R_IER                 0x18
#define R_TXDP                0x20
#define R_TXCFG               0x24
#define R_RXDP                0x30
#define R_RXCFG               0x34
#define R_CCSR                0x3C
#define R_WCSR                0x40
#define R_PCR                 0x44
#define R_RFCR                0x48
#define R_RFDR                0x4C
#define R_BRAR                0x50
#define R_BRDR                0x54
#define R_SRR                 0x58
#define R_MIBC                0x5C
#define R_MIB(n)              (0x60+4*(n))

/*  MacPhyter MIB Registers */

#define R_MIB_RXErroredPkts   R_MIB(0)
#define R_MIB_RXFCSErrors     R_MIB(1)
#define R_MIB_RXMsdPktErrors  R_MIB(2)
#define R_MIB_RXFAErrors      R_MIB(3)
#define R_MIB_RXSymbolErrors  R_MIB(4)
#define R_MIB_RXFrameTooLong  R_MIB(5)
#define R_MIB_TXSQEErrors     R_MIB(6)

#define N_MIB_REGISTERS       7


/*  MacPhyter Internal PHY Register offsets */

#define R_BMCR                0x80
#define R_BMSR                0x84
#define R_PHYIDR1             0x88
#define R_PHYIDR2             0x8C
#define R_ANAR                0x90
#define R_ANLPAR              0x94
#define R_ANER                0x98
#define R_ANNPTR              0x9C
#define R_PHYSTS              0xC0
#define R_MICR                0xC4
#define R_MISR                0xC8
#define R_FCSCR               0xD0
#define R_RECR                0xD4
#define R_PHYCR               0xE4
#define R_TBTSCR              0xE8

/* Undocumented, updated for CVNG (SRR=0x0302) parts */

#define R_PGSEL               0xCC
#define R_PMDCSR              0xE4   /* R_PHYCR */
#define R_DSPCFG              0xF4
#define R_SDCFG               0xF8
#define R_TSTDAT              0xFC


/* 0x00  CR: Command Register */

#define M_CR_TXE                _DD_MAKEMASK1(0)
#define M_CR_TXD                _DD_MAKEMASK1(1)
#define M_CR_RXE                _DD_MAKEMASK1(2)
#define M_CR_RXD                _DD_MAKEMASK1(3)
#define M_CR_TXR                _DD_MAKEMASK1(4)
#define M_CR_RXR                _DD_MAKEMASK1(5)
#define M_CR_SWI                _DD_MAKEMASK1(7)
#define M_CR_RST                _DD_MAKEMASK1(8)


/* 0x04  CFG: Configuration and Media Status Register */

#define M_CFG_BEM               _DD_MAKEMASK1(0)
#define M_CFG_BROM_DIS          _DD_MAKEMASK1(2)
#define M_CFG_PESEL             _DD_MAKEMASK1(3)
#define M_CFG_EXD               _DD_MAKEMASK1(4)
#define M_CFG_POW               _DD_MAKEMASK1(5)
#define M_CFG_SB                _DD_MAKEMASK1(6)
#define M_CFG_REQALG            _DD_MAKEMASK1(7)
#define M_CFG_EUPHCOMP          _DD_MAKEMASK1(8)
#define M_CFG_PHY_DIS           _DD_MAKEMASK1(9)
#define M_CFG_PHY_RST           _DD_MAKEMASK1(10)
#define M_CFG_EXT_PHY           _DD_MAKEMASK1(12)

#define S_CFG_ANEG_SEL          13
#define M_CFG_ANEG_SEL          _DD_MAKEMASK(3,S_CFG_ANEG_SEL)
#define V_CFG_ANEG_SEL(x)       _DD_MAKEVALUE(x,S_CFG_ANEG_SEL)
#define G_CFG_ANEG_SEL(x)       _DD_GETVALUE(x,S_CFG_ANEG_SEL,M_CFG_ANEG_SEL)

#define K_ANEG_10H              0x0
#define K_ANEG_100H             0x2
#define K_ANEG_10F              0x4
#define K_ANEG_100F             0x6
#define K_ANEG_10HF             0x1
#define K_ANEG_10H_100H         0x3
#define K_ANEG_100HF            0x5
#define K_ANEG_10HF_100HF       0x7
#define K_ANEG_ALL              0x7

#define M_CFG_PAUSE_ADV         _DD_MAKEMASK1(16)
#define M_CFG_PINT_ACEN         _DD_MAKEMASK1(17)

#define S_CFG_PHY_CFG           18
#define M_CFG_PHY_CFG           _DD_MAKEMASK(6,S_CFG_PHY_CFG)

#define M_CFG_ANEG_DN           _DD_MAKEMASK1(27)
#define M_CFG_POL               _DD_MAKEMASK1(28)
#define M_CFG_FDUP              _DD_MAKEMASK1(29)
#define M_CFG_SPEED100          _DD_MAKEMASK1(30)
#define M_CFG_LNKSTS            _DD_MAKEMASK1(31)
#define M_CFG_LNKSUMMARY        (M_CFG_LNKSTS | M_CFG_SPEED100 | M_CFG_FDUP)


/* 0x08  MEAR: EEPROM Access Register (see 4.2.4 for EEPROM Map) */

#define M_MEAR_EEDI             _DD_MAKEMASK1(0)
#define M_MEAR_EEDO             _DD_MAKEMASK1(1)
#define M_MEAR_EECLK            _DD_MAKEMASK1(2)
#define M_MEAR_EESEL            _DD_MAKEMASK1(3)
#define M_MEAR_MDIO             _DD_MAKEMASK1(4)
#define M_MEAR_MDDIR            _DD_MAKEMASK1(5)
#define M_MEAR_MDC              _DD_MAKEMASK1(6)


/* 0x0C  PTSCR: PCI Test Control Register */

#define M_PTSCR_EEBIST_FAIL     _DD_MAKEMASK1(0)
#define M_PTSCR_EEBIST_EN       _DD_MAKEMASK1(1)
#define M_PTSCR_EELOAD_EN       _DD_MAKEMASK1(2)
#define M_PTSCR_RBIST_RXFFAIL   _DD_MAKEMASK1(3)
#define M_PTSCR_RBIST_TXFAIL    _DD_MAKEMASK1(4)
#define M_PTSCR_RBIST_RXFAIL    _DD_MAKEMASK1(5)
#define M_PTSCR_RBIST_DONE      _DD_MAKEMASK1(6)
#define M_PTSCR_RBIST_EN        _DD_MAKEMASK1(7)
#define M_PTSCR_MBZ8            _DD_MAKEMASK1(8)
#define M_PTSCR_MBZ9            _DD_MAKEMASK1(9)
#define M_PTSCR_RBIST_RST       _DD_MAKEMASK1(10)
#define M_PTSCR_MBZ12           _DD_MAKEMASK1(12)


/* 0x10  ISR: Interrupt Status Register */
/* 0x14  IMR: Interrupt Mask Register */

#define M_INT_RXOK              _DD_MAKEMASK1(0)
#define M_INT_RXDESC            _DD_MAKEMASK1(1)
#define M_INT_RXERR             _DD_MAKEMASK1(2)
#define M_INT_RXEARLY           _DD_MAKEMASK1(3)
#define M_INT_RXIDLE            _DD_MAKEMASK1(4)
#define M_INT_RXORN             _DD_MAKEMASK1(5)
#define M_INT_TXOK              _DD_MAKEMASK1(6)
#define M_INT_TXDESC            _DD_MAKEMASK1(7)
#define M_INT_TXERR             _DD_MAKEMASK1(8)
#define M_INT_TXIDLE            _DD_MAKEMASK1(9)
#define M_INT_TXURN             _DD_MAKEMASK1(10)
#define M_INT_MIB               _DD_MAKEMASK1(11)
#define M_INT_SWI               _DD_MAKEMASK1(12)
#define M_INT_PME               _DD_MAKEMASK1(13)
#define M_INT_PHY               _DD_MAKEMASK1(14)
#define M_INT_HIBERR            _DD_MAKEMASK1(15)
#define M_INT_RXSOVR            _DD_MAKEMASK1(16)
#define M_INT_RTABT             _DD_MAKEMASK1(20)
#define M_INT_RMABT             _DD_MAKEMASK1(21)
#define M_INT_SSERR             _DD_MAKEMASK1(22)
#define M_INT_DPERR             _DD_MAKEMASK1(23)
#define M_INT_RXRCMP            _DD_MAKEMASK1(24)
#define M_INT_TXRCMP            _DD_MAKEMASK1(25)


/* 0x18  IER: Interrupt Enable Register */

#define M_IER_IE                _DD_MAKEMASK1(0)


/* 0x20  TXDP: Transmit Descriptor Pointer Register */


/* 0x24  TXCFG: Transmit Configuration Register */

#define S_TXCFG_DRTH            0
#define M_TXCFG_DRTH            _DD_MAKEMASK(6,S_TXCFG_DRTH)
#define V_TXCFG_DRTH(x)         _DD_MAKEVALUE(x,S_TXCFG_DRTH)
#define G_TXCFG_DRTH(x)         _DD_GETVALUE(x,S_TXCFG_DRTH,M_TXCFG_DRTH)

#define S_TXCFG_FLTH            8
#define M_TXCFG_FLTH            _DD_MAKEMASK(6,S_TXCFG_FLTH)
#define V_TXCFG_FLTH(x)         _DD_MAKEVALUE(x,S_TXCFG_FLTH)
#define G_TXCFG_FLTH(x)         _DD_GETVALUE(x,S_TXCFG_FLTH,M_TXCFG_FLTH)

#define S_TXCFG_MXDMA           20
#define M_TXCFG_MXDMA           _DD_MAKEMASK(3,S_TXCFG_MXDMA)
#define V_TXCFG_MXDMA(x)        _DD_MAKEVALUE(x,S_TXCFG_MXDMA)
#define G_TXCFG_MXDMA(x)        _DD_GETVALUE(x,S_TXCFG_MXDMA,M_TXCFG_MXDMA)

/* Max DMA burst size (bytes) - RX also */
#define K_MXDMA_512             0x0
#define K_MXDMA_4               0x1
#define K_MXDMA_8               0x2
#define K_MXDMA_16              0x3
#define K_MXDMA_32              0x4
#define K_MXDMA_64              0x5
#define K_MXDMA_128             0x6
#define K_MXDMA_256             0x7

#define M_TXCFG_ECRETRY         _DD_MAKEMASK1(23)

#define S_TXCFG_IFG             26
#define M_TXCFG_IFG             _DD_MAKEMASK(2,S_TXCFG_IFG)
#define V_TXCFG_IFG(x)          _DD_MAKEVALUE(x,S_TXCFG_IFG)
#define G_TXCFG_IFG(x)          _DD_GETVALUE(x,S_TXCFG_IFG,M_TXCFG_IFG)

#define M_TXCFG_ATP             _DD_MAKEMASK1(28)
#define M_TXCFG_MLB             _DD_MAKEMASK1(29)
#define M_TXCFG_HBI             _DD_MAKEMASK1(30)
#define M_TXCFG_CSI             _DD_MAKEMASK1(31)


/* 0x30  RXDP: Receive Descriptor Pointer Register */


/* 0x34  RXCFG: Receive Configuration Register */

#define S_RXCFG_DRTH            1
#define M_RXCFG_DRTH            _DD_MAKEMASK(5,S_RXCFG_DRTH)
#define V_RXCFG_DRTH(x)         _DD_MAKEVALUE(x,S_RXCFG_DRTH)
#define G_RXCFG_DRTH(x)         _DD_GETVALUE(x,S_RXCFG_DRTH,M_RXCFG_DRTH)

#define S_RXCFG_MXDMA           20
#define M_RXCFG_MXDMA           _DD_MAKEMASK(3,S_RXCFG_MXDMA)
#define V_RXCFG_MXDMA(x)        _DD_MAKEVALUE(x,S_RXCFG_MXDMA)
#define G_RXCFG_MXDMA(x)        _DD_GETVALUE(x,S_RXCFG_MXDMA,M_RXCFG_MXDMA)

#define M_RXCFG_ALP             _DD_MAKEMASK1(27)
#define M_RXCFG_ATX             _DD_MAKEMASK1(28)
#define M_RXCFG_ARP             _DD_MAKEMASK1(30)
#define M_RXCFG_AEP             _DD_MAKEMASK1(31)


/* 0x3C  CCSR: CLKRUN Control/Status Register */

#define M_CCSR_CLKRUN_EN        _DD_MAKEMASK1(0)
#define M_CCSR_PMEEN            _DD_MAKEMASK1(8)
#define M_CCSR_PMESTS           _DD_MAKEMASK1(15)


/* 0x40  WCSR: Wake Command/Status Register */

#define M_WCSR_WKPHY            _DD_MAKEMASK1(0)
#define M_WCSR_WKUCP            _DD_MAKEMASK1(1)
#define M_WCSR_WKMCP            _DD_MAKEMASK1(2)
#define M_WCSR_WKBCP            _DD_MAKEMASK1(3)
#define M_WCSR_WKARP            _DD_MAKEMASK1(4)
#define M_WCSR_WKPAT0           _DD_MAKEMASK1(5)
#define M_WCSR_WKPAT1           _DD_MAKEMASK1(6)
#define M_WCSR_WKPAT2           _DD_MAKEMASK1(7)
#define M_WCSR_WKPAT3           _DD_MAKEMASK1(8)
#define M_WCSR_WKMAG            _DD_MAKEMASK1(9)
#define M_WCSR_MPSOE            _DD_MAKEMASK1(10)
#define M_WCSR_SOHACK           _DD_MAKEMASK1(20)
#define M_WCSR_PHYINT           _DD_MAKEMASK1(22)
#define M_WCSR_UCASTR           _DD_MAKEMASK1(23)
#define M_WCSR_MCASTR           _DD_MAKEMASK1(24)
#define M_WCSR_BCASTR           _DD_MAKEMASK1(25)
#define M_WCSR_ARPR             _DD_MAKEMASK1(26)
#define M_WCSR_PATM0            _DD_MAKEMASK1(27)
#define M_WCSR_PATM1            _DD_MAKEMASK1(28)
#define M_WCSR_PATM2            _DD_MAKEMASK1(29)
#define M_WCSR_PATM3            _DD_MAKEMASK1(30)
#define M_WCSR_MPR              _DD_MAKEMASK1(31)


/* 0x44  PCR: Pause Control/Status Register */

#define S_PCR_PAUSE_CNT         0
#define M_PCR_PAUSE_CNT         _DD_MAKEMASK(16,S_PCR_PAUSE_CNT)
#define V_PCR_PAUSE_CNT(x)      _DD_MAKEVALUE(x,S_PCR_PAUSE_CNT)
#define G_PCR_PAUSE_CNT(x)      _DD_GETVALUE(x,S_PCR_PAUSE_CNT,M_PCR_PAUSE_CNT)

#define M_PCR_MLD_EN            _DD_MAKEMASK1(16)
#define M_PCR_PSNEG             _DD_MAKEMASK1(21)
#define M_PCR_PS_RCVD           _DD_MAKEMASK1(22)
#define M_PCR_PS_ACT            _DD_MAKEMASK1(23)
#define M_PCR_PS_DA             _DD_MAKEMASK1(29)
#define M_PCR_PS_MCAST          _DD_MAKEMASK1(30)
#define M_PCR_PSEN              _DD_MAKEMASK1(31)


/* 0x48  RFCR: Receive Filter/Match Control Register */


#define S_RFCR_RFADDR           0
#define M_RFCR_RFADDR           _DD_MAKEMASK(10,S_RFCR_RFADDR)
#define V_RFCR_RFADDR(x)        _DD_MAKEVALUE(x,S_RFCR_RFADDR)
#define G_RFCR_RFADDR(x)        _DD_GETVALUE(x,S_RFCR_RFADDR,M_RFCR_RFADDR)

#define K_RFCR_PMATCH_ADDR      0x000
#define K_RFCR_PCOUNT_ADDR      0x006
#define K_RFCR_FILTER_ADDR      0x200

#define M_RFCR_ULM              _DD_MAKEMASK1(19)
#define M_RFCR_UHEN             _DD_MAKEMASK1(20)
#define M_RFCR_MHEN             _DD_MAKEMASK1(21)
#define M_RFCR_AARP             _DD_MAKEMASK1(22)
#define M_RFCR_APAT0            _DD_MAKEMASK1(23)
#define M_RFCR_APAT1            _DD_MAKEMASK1(24)
#define M_RFCR_APAT2            _DD_MAKEMASK1(25)
#define M_RFCR_APAT3            _DD_MAKEMASK1(26)
#define M_RFCR_APAT             (M_RFCR_APAT0 | M_RFCR_APAT1 | \
                                 M_RFCR_APAT2 | M_RFCR_APAT3 )
#define M_RFCR_APM              _DD_MAKEMASK1(27)
#define M_RFCR_AAU              _DD_MAKEMASK1(28)
#define M_RFCR_AAM              _DD_MAKEMASK1(29)
#define M_RFCR_AAB              _DD_MAKEMASK1(30)
#define M_RFCR_RFEN             _DD_MAKEMASK1(31)


/* 0x4C  RFDR: Receive Filter/Match Data Register */

#define S_RFDR_RFDATA           0
#define M_RFDR_RFDATA           _DD_MAKEMASK(16,S_RFDR_RFDATA)
#define V_RFDR_RFDATA(x)        _DD_MAKEVALUE(x,S_RFDR_RFDATA)
#define G_RFDR_RFDATA(x)        _DD_GETVALUE(x,S_RFDR_RFDATA,M_RFDR_RFDATA)

#define S_RFDR_BMASK            16
#define M_RFDR_BMASK            _DD_MAKEMASK(2,S_RFDR_BMASK)
#define V_RFDR_BMASK(x)         _DD_MAKEVALUE(x,S_RFDR_BMASK)
#define G_RFDR_BMASK(x)         _DD_GETVALUE(x,S_RFDR_BMASK,M_RFDR_BMASK)


/* 0x50  BRAR: Boot ROM Address Register */

#define S_BRAR_ADDR             0
#define M_BRAR_ADDR             _DD_MAKEMASK(16,S_BRAR_ADDR)
#define V_BRAR_ADDR(x)          _DD_MAKEVALUE(x,S_BRAR_ADDR)
#define G_BRAR_ADDR(x)          _DD_GETVALUE(x,S_BRAR_ADDR,M_BRAR_ADDR)

#define M_BRAR_AUTOINC          _DD_MAKEMASK1(31)


/* 0x54  BRDR: Boot ROM Data Register */


/* 0x58  SRR: Silicon Revision Register */

#define S_SRR_REV               0
#define M_SRR_REV               _DD_MAKEMASK(16,S_SRR_REV)
#define V_SRR_REV(x)            _DD_MAKEVALUE(x,S_SRR_REV)
#define G_SRR_REV(x)            _DD_GETVALUE(x,S_SRR_REV,M_SRR_REV)

#define K_REV_CVNG              0x00000302
#define K_REV_DVNG_UJB          0x00000403


/* 0x5C  MIBC: Management Information Base Control Register */

#define M_MIBC_WRN              _DD_MAKEMASK1(0)
#define M_MIBC_FRZ              _DD_MAKEMASK1(1)
#define M_MIBC_ACLR             _DD_MAKEMASK1(2)
#define M_MIBC_MIBS             _DD_MAKEMASK1(3)


/* MIB Counters */

/* 0x60  RXErroredPkts  */
/* 0x64  RXFCSErrors    */
/* 0x68  RXMsdPktErrors */
/* 0x6C  RXFAErrors     */
/* 0x70  RXSymbolErrors */
/* 0x74  RXFrameTooLong */
/* 0x78  TXSQEErrors    */


/* See ../net/mii.h for fields of standard (MII) PHY registers */

#define K_83815_PHYID1           0x2000
#define K_83815_PHYID2           0x5C21

#define K_ANNPTR_NULL            0x0001


/* 0xC0  PHYSTS: PHY Status Register */

#define PHYSTS_RXERRLATCH        0x2000
#define PHYSTS_POLARITYSTAT      0x1000
#define PHYSTS_FALSECARRLATCH    0x0800
#define PHYSTS_SIGNALDETECT      0x0400
#define PHYSTS_DESCRAMBLOCK      0x0200
#define PHYSTS_PAGERECVD         0x0100
#define PHYSTS_MIIINT            0x0080
#define PHYSTS_REMOTEFAULT       0x0040
#define PHYSTS_JABBERDET         0x0020
#define PHYSTS_ANCOMPLETE        0x0010
#define PHYSTS_LOOPBACK          0x0008
#define PHYSTS_DUPLEX            0x0004
#define PHYSTS_SPEED10           0x0002
#define PHYSTS_LINKSTAT          0x0001


/* 0xC4  MICR: MII Interrupt Control Register */

#define MICR_INTEN               0x0002
#define MICR_TINT                0x0001


/* 0xC8  MISR: MII Interrupt Status and Misc. Control Register */

#define MISR_MINT                0x8000
#define MISR_MSKLINK             0x4000
#define MISR_MSKJAB              0x2000
#define MISR_MSKRF               0x1000
#define MISR_MSKANC              0x0800
#define MISR_MSKFHF              0x0400
#define MISR_MSKRHF              0x0200


/* 0xD0  FCSCR: False Carrier Sense Counter Register */

#define FCSCR_FCSCNT             0x00FF


/* 0xD4  RECR: Receiver Error Counter Register */

#define RECR_RXERCNT             0x00FF


/* 0xD8  PCSR: 100 Mb/s PCS Configuration and Status Register */

#define PCSR_BYP4B5B             0x1000
#define PCSR_FREECLK             0x0800
#define PCSR_TQEN                0x0400
#define PCSR_SDFORCEB            0x0200
#define PCSR_SDOPTION            0x0100
#define PCSR_FORCE100OK          0x0020
#define PCSR_NRZIBYPASS          0x0004


/* 0xE4  PHYCR: PHY Control Register */

#define PHYCR_PSR15              0x0800
#define PHYCR_BISTSTATUS         0x0400
#define PHYCR_BISTSTART          0x0200
#define PHYCR_BPSTRETCH          0x0100
#define PHYCR_PAUSESTS           0x0080
#define PHYCR_PHYADDR            0x001F


/* 0xE8  TBTSCR: 10Base-T Status/Control Register */

#define TBTSCR_LPBK10DIS         0x0100
#define TBTSCR_LPDIS             0x0080
#define TBTSCR_FORCELINK10       0x0040
#define TBTSCR_FORCEPOLCOR       0x0020
#define TBTSCR_POLARITY          0x0010
#define TBTSCR_AUTOPOLDIS        0x0008
#define TBTSCR_HBDIS             0x0002
#define TBTSCR_JABBERDIS         0x0001


/*  MacPhyter Transmit and Receive Descriptors */

/* Common Command/Status Fields */
#define S_DES1_SIZE              0
#define M_DES1_SIZE              _DD_MAKEMASK(12,S_DES1_SIZE)
#define V_DES1_SIZE(x)           _DD_MAKEVALUE(x,S_DES1_SIZE)
#define G_DES1_SIZE(x)           _DD_GETVALUE(x,S_DES1_SIZE,M_DES1_SIZE)

#define M_DES1_OK                _DD_MAKEMASK1(27)
#define M_DES1_INTR              _DD_MAKEMASK1(29)
#define M_DES1_MORE              _DD_MAKEMASK1(30)
#define M_DES1_OWN               _DD_MAKEMASK1(31)

/* Transmit Command/Status Bits */
#define S_DES1_CCNT              16
#define M_DES1_CCNT              _DD_MAKEMASK(4,S_DES1_CCNT)
#define V_DES1_CCNT(x)           _DD_MAKEVALUE(x,S_DES1_CCNT)
#define G_DES1_CCNT(x)           _DD_GETVALUE(x,S_DES1_CCNT,M_DES1_CCNT)

#define M_DES1_EC                _DD_MAKEMASK1(20)
#define M_DES1_OWC               _DD_MAKEMASK1(21)
#define M_DES1_ED                _DD_MAKEMASK1(22)
#define M_DES1_TD                _DD_MAKEMASK1(23)
#define M_DES1_CRS               _DD_MAKEMASK1(24)
#define M_DES1_TFU               _DD_MAKEMASK1(25)
#define M_DES1_TXA               _DD_MAKEMASK1(26)
#define M_DES1_SUPCRC            _DD_MAKEMASK1(28)

/* Receive Command/Status Bits */
#define M_DES1_COL               _DD_MAKEMASK1(16)
#define M_DES1_LBP               _DD_MAKEMASK1(17)
#define M_DES1_FAE               _DD_MAKEMASK1(18)
#define M_DES1_CRCE              _DD_MAKEMASK1(19)
#define M_DES1_ISE               _DD_MAKEMASK1(20)
#define M_DES1_RUNT              _DD_MAKEMASK1(21)
#define M_DES1_LONG              _DD_MAKEMASK1(22)
#define M_DES1_RX_ERRORS         (M_DES1_CRCE |                           \
                                  M_DES1_COL | M_DES1_FAE | M_DES1_ISE |  \
                                  M_DES1_RUNT | M_DES1_LONG | M_DES1_RXO)

#define S_DES1_DEST              23
#define M_DES1_DEST              _DD_MAKEMASK(2,S_DES1_DEST)
#define V_DES1_DEST(x)           _DD_MAKEVALUE(x,S_DES1_DEST)
#define G_DES1_DEST(x)           _DD_GETVALUE(x,S_DES1_DEST,M_DES1_DEST)

#define K_DEST_REJECT            0
#define K_DEST_UNICAST           1
#define K_DEST_MULTICAST         2
#define K_DEST_BROADCAST         3

#define M_DES1_RXO               _DD_MAKEMASK1(25)
#define M_DES1_RXA               _DD_MAKEMASK1(26)
#define M_DES1_INCCRC            _DD_MAKEMASK1(28)

#endif /* _DP83815_H_ */
