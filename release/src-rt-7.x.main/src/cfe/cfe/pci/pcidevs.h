/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	pcidevs 2002/09/03 broadcom 
 */

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NOTE: a fairly complete list of PCI codes can be found at:
 *
 *	http://members.hyperlink.net.au/~chart/pci.htm
 *
 * which replaces the database found at
 *
 *	http://www.yourvote.com/pci/
 *
 * (but it doesn't always seem to match vendor documentation)
 */

/*
 * List of known PCI vendors.  This list has been trimmed to include
 * only vendors with products appearing in the lists below.
 */

#define	PCI_VENDOR_COMPAQ	0x0e11		/* Compaq */
#define	PCI_VENDOR_SYMBIOS	0x1000		/* Symbios Logic */
#define	PCI_VENDOR_ATI	0x1002		/* ATI Technologies */
#define	PCI_VENDOR_ULSI	0x1003		/* ULSI Systems */
#define	PCI_VENDOR_VLSI	0x1004		/* VLSI Technology */
#define	PCI_VENDOR_AVANCE	0x1005		/* Avance Logic */
#define	PCI_VENDOR_NS	0x100b		/* National Semiconductor */
#define	PCI_VENDOR_TSENG	0x100c		/* Tseng Labs */
#define	PCI_VENDOR_WEITEK	0x100e		/* Weitek */
#define	PCI_VENDOR_DEC	0x1011		/* Digital Equipment */
#define	PCI_VENDOR_CIRRUS	0x1013		/* Cirrus Logic */
#define	PCI_VENDOR_IBM	0x1014		/* IBM */
#define	PCI_VENDOR_WD	0x101c		/* Western Digital */
#define	PCI_VENDOR_AMD	0x1022		/* Advanced Micro Devices */
#define	PCI_VENDOR_TRIDENT	0x1023		/* Trident Microsystems */
#define	PCI_VENDOR_ACER	0x1025		/* Acer */
#define	PCI_VENDOR_MATROX	0x102b		/* Matrox */
#define	PCI_VENDOR_CHIPS	0x102c		/* Chips and Technologies */
#define	PCI_VENDOR_TOSHIBA	0x102f		/* Toshiba America */
#define	PCI_VENDOR_NEC	0x1033		/* NEC */
#define	PCI_VENDOR_FUTUREDOMAIN	0x1036		/* Future Domain */
#define	PCI_VENDOR_SIS	0x1039		/* Silicon Integrated System */
#define	PCI_VENDOR_HP	0x103c		/* Hewlett-Packard */
#define	PCI_VENDOR_PCTECH	0x1042		/* PC Technology */
#define	PCI_VENDOR_DPT	0x1044		/* Distributed Processing Technology */
#define	PCI_VENDOR_OPTI	0x1045		/* Opti */
#define	PCI_VENDOR_ELSA	0x1048		/* Elsa */
#define	PCI_VENDOR_SGSTHOMSON	0x104a		/* SGS Thomson Microelectric */
#define	PCI_VENDOR_BUSLOGIC	0x104b		/* BusLogic */
#define	PCI_VENDOR_TI	0x104c		/* Texas Instruments */
#define	PCI_VENDOR_SONY	0x104d		/* Sony */
#define	PCI_VENDOR_OAKTECH	0x104e		/* Oak Technology */
#define	PCI_VENDOR_WINBOND	0x1050		/* Winbond Electronics */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_PROMISE	0x105a		/* Promise Technology */
#define	PCI_VENDOR_NUMBER9	0x105d		/* Number 9 Computer Company */
#define	PCI_VENDOR_UMC	0x1060		/* United Microelectronics */
#define	PCI_VENDOR_ITT	0x1061		/* I. T. T. */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_APPLE	0x106b		/* Apple Computer */
#define	PCI_VENDOR_YAMAHA	0x1073		/* Yamaha */
#define	PCI_VENDOR_NEXGEN	0x1074		/* NexGen Microsystems */
#define	PCI_VENDOR_QLOGIC	0x1077		/* Q Logic */
#define	PCI_VENDOR_LEADTEK	0x107d		/* LeadTek Research */
#define	PCI_VENDOR_CONTAQ	0x1080		/* Contaq Microsystems */
#define	PCI_VENDOR_BIT3	0x108a		/* Bit3 Computer Corp. */
#define	PCI_VENDOR_OLICOM	0x108d		/* Olicom */
#define	PCI_VENDOR_SUN	0x108e		/* Sun Microsystems */
#define	PCI_VENDOR_INTERGRAPH	0x1091		/* Intergraph */
#define	PCI_VENDOR_DIAMOND	0x1092		/* Diamond Computer Systems */
#define	PCI_VENDOR_CMDTECH	0x1095		/* CMD Technology */
#define	PCI_VENDOR_QUANTUMDESIGNS	0x1098		/* Quantum Designs */
#define	PCI_VENDOR_BROOKTREE	0x109e		/* Brooktree */
#define	PCI_VENDOR_SGI	0x10a9		/* Silicon Graphics */
#define	PCI_VENDOR_ACC	0x10aa		/* ACC Microelectronics */
#define	PCI_VENDOR_SYMPHONY	0x10ad		/* Symphony Labs */
#define	PCI_VENDOR_PLX	0x10b5		/* PLX Technology */
#define	PCI_VENDOR_MADGE	0x10b6		/* Madge Networks */
#define	PCI_VENDOR_3COM	0x10B7		/* 3Com */
#define	PCI_VENDOR_SMC	0x10b8		/* Standard Microsystems */
#define	PCI_VENDOR_ALI	0x10b9		/* Acer Labs */
#define	PCI_VENDOR_SURECOM	0x10bd		/* Surecom Technology */
#define	PCI_VENDOR_SAMSUNGSEMI	0x10c3		/* Samsung Semiconductors */
#define	PCI_VENDOR_NEOMAGIC	0x10c8		/* Neomagic */
#define	PCI_VENDOR_ADVSYS	0x10cd		/* Advanced System Products */
#define	PCI_VENDOR_MACRONIX	0x10d9		/* Macronix */
#define	PCI_VENDOR_ES	0x10dd		/* Evans & Sutherland */
#define	PCI_VENDOR_NVIDIA	0x10de		/* Nvidia Corporation */
#define	PCI_VENDOR_EMULEX	0x10df		/* Emulex */
#define	PCI_VENDOR_IMS	0x10e0		/* Integrated Micro Solutions */
#define	PCI_VENDOR_TEKRAM	0x10e1		/* Tekram Technology (1st PCI Vendor ID) */
#define	PCI_VENDOR_NEWBRIDGE	0x10e3		/* Newbridge Microsystems / Tundra Semiconductor */
#define	PCI_VENDOR_AMCIRCUITS	0x10e8		/* Applied Micro Circuits */
#define	PCI_VENDOR_REALTEK	0x10ec		/* Realtek Semiconductor */
#define	PCI_VENDOR_NKK	0x10f5		/* NKK Corporation */
#define	PCI_VENDOR_INITIO	0x1101		/* Initio */
#define	PCI_VENDOR_CREATIVELABS	0x1102		/* Creative Labs */
#define	PCI_VENDOR_TRIONES	0x1103		/* Triones Technologies */
#define	PCI_VENDOR_SIGMA	0x1105		/* Sigma Designs */
#define	PCI_VENDOR_VIATECH	0x1106		/* VIA Technologies */
#define	PCI_VENDOR_COGENT	0x1109		/* Cogent Data Technologies */
#define	PCI_VENDOR_RNS	0x1112		/* RNS */
#define	PCI_VENDOR_ACCTON	0x1113		/* Accton Technology */
#define	PCI_VENDOR_VORTEX	0x1119		/* Vortex Computer Systems */
#define	PCI_VENDOR_EFFICIENTNETS	0x111a		/* Efficent Networks */
#define	PCI_VENDOR_IDT	0x111d		/* IDT */
#define	PCI_VENDOR_FORE	0x1127		/* FORE Systems */
#define	PCI_VENDOR_ZIATECH	0x1138		/* Ziatech */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance Semiconductor */
#define	PCI_VENDOR_SCHNEIDERKOCH	0x1148		/* Schneider & Koch */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	PCI_VENDOR_ALTERA	0x1172		/* Altera Corporation */
#define	PCI_VENDOR_TOSHIBA2	0x1179		/* Toshiba America Info Systems */
#define	PCI_VENDOR_RICOH	0x1180		/* Ricoh */
#define	PCI_VENDOR_DLINK	0x1186		/* D-Link Systems */
#define	PCI_VENDOR_COROLLARY	0x118c		/* Corrollary */
#define	PCI_VENDOR_ACARD	0x1191		/* Acard */
#define	PCI_VENDOR_ZEINET	0x1193		/* Zeinet */
#define	PCI_VENDOR_GALILEO	0x11ab		/* Galileo Technology */
#define	PCI_VENDOR_LITEON	0x11ad		/* Lite-On Communications */
#define	PCI_VENDOR_V3	0x11b0		/* V3 Semiconductor */
#define	PCI_VENDOR_LUCENT	0x11c1		/* AT&T Microelectronics */
#define	PCI_VENDOR_DOLPHIN	0x11c8		/* Dolphin Interconnect Solutions */
#define	PCI_VENDOR_AURAVISION	0x11d1		/* Auravision */
#define	PCI_VENDOR_ZORAN	0x11de		/* Zoran Corporation */
#define	PCI_VENDOR_COMPEX	0x11f6		/* Compex */
#define	PCI_VENDOR_PMCSIERRA	0x11f8		/* PMC-Sierra */
#define	PCI_VENDOR_CYCLADES	0x120e		/* Cyclades */
#define	PCI_VENDOR_ESSENTIAL	0x120f		/* Essential Communications */
#define	PCI_VENDOR_O2MICRO	0x1217		/* O2 Micro Inc */
#define	PCI_VENDOR_3DFX	0x121a		/* 3Dfx Interactive */
#define	PCI_VENDOR_CCUBE	0x123f		/* C-Cube Microsystems */
#define	PCI_VENDOR_AVM	0x1244		/* AVM */
#define	PCI_VENDOR_LINEARSYS	0x1254		/* Linear Systems */
#define	PCI_VENDOR_ASIX	0x125b		/* ASIX Electronics */
#define	PCI_VENDOR_ESSTECH	0x125d		/* ESS Technology Inc */
#define	PCI_VENDOR_SILMOTION	0x126f		/* Silicon Motion */
#define	PCI_VENDOR_ENSONIQ	0x1274		/* Ensoniq */
#define	PCI_VENDOR_DAVICOM	0x1282		/* Davicom Semiconductor */
#define	PCI_VENDOR_ESSTECH2	0x1285		/* ESS Technology Inc */
#define	PCI_VENDOR_TRITECH	0x1292		/* TriTech Microelectronics */
#define	PCI_VENDOR_ALTEON	0x12ae		/* Alteon */
#define	PCI_VENDOR_RISCOM	0x12aa		/* RISCom */
#define	PCI_VENDOR_USR	0x12b9		/* US Robotics (3Com) */
#define	PCI_VENDOR_NVIDIA_SGS	0x12d2		/* Nvidia Corporation & SGS Thomson Microelectric */
#define	PCI_VENDOR_AUREAL	0x12eb		/* Aureal Semiconductor */
#define	PCI_VENDOR_ADMTEK	0x1317		/* ADMtek */
#define	PCI_VENDOR_FORTEMEDIA	0x1319		/* Forte Media */
#define	PCI_VENDOR_DOMEX	0x134a		/* Domex */
#define	PCI_VENDOR_LMC	0x1376		/* LAN Media Corporation */
#define	PCI_VENDOR_API	0x14d9		/* API Networks */
#define	PCI_VENDOR_CONEXANT	0x14f1		/* Conexant Systems */
#define	PCI_VENDOR_NETGEAR	0x1385		/* Netgear */
#define	PCI_VENDOR_3WARE	0x13c1		/* 3ware */
#define	PCI_VENDOR_SUNDANCETI	0x13f0		/* Sundance Technology */
#define	PCI_VENDOR_CMEDIA	0x13f6		/* C-Media Electronics Inc */
#define	PCI_VENDOR_DELTA	0x1500		/* Delta Electronics */
#define	PCI_VENDOR_SOLIDUM	0x1588		/* Solidum Systems Corp. */
#define	PCI_VENDOR_SIBYTE	0x166d		/* SiByte, Inc. */
#define	PCI_VENDOR_SYMPHONY2	0x1c1c		/* Symphony Labs (2nd PCI Vendor ID) */
#define	PCI_VENDOR_TEKRAM2	0x1de1		/* Tekram Technology (2nd PCI Vendor ID) */
#define	PCI_VENDOR_BROADCOM	0x14e4		/* Broadcom */
#define	PCI_VENDOR_3DLABS	0x3d3d		/* 3D Labs */
#define	PCI_VENDOR_AVANCE2	0x4005		/* Avance Logic (2nd PCI Vendor ID) */
#define	PCI_VENDOR_ADDTRON	0x4033		/* Addtron Technology */
#define	PCI_VENDOR_NETVIN	0x4a14		/* NetVin */
#define	PCI_VENDOR_S3	0x5333		/* S3 */
#define	PCI_VENDOR_C4T	0x6374		/* c't Magazin */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_PROLAN	0x8c4a		/* ProLAN */
#define	PCI_VENDOR_KTI	0x8e2e		/* KTI */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ADP2	0x9005		/* Adaptec (2nd PCI Vendor ID) */
#define	PCI_VENDOR_ATRONICS	0x907f		/* Atronics */
#define	PCI_VENDOR_ARC	0xedd8		/* ARC Logic */
#define	PCI_VENDOR_EPIGRAM	0xfeda		/* Epigram */
#define	PCI_VENDOR_INVALID	0xffff		/* INVALID VENDOR ID */

/*
 * List of known products.  Grouped by vendor.
 */

/* 3COM Products */
#define	PCI_PRODUCT_3COM_3C985	0x0001		/* 3c985 Gigabit Ethernet */
#define	PCI_PRODUCT_3COM_3C590	0x5900		/* 3c590 Ethernet */
#define	PCI_PRODUCT_3COM_3C595TX	0x5950		/* 3c595-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C595T4	0x5951		/* 3c595-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C595MII	0x5952		/* 3c595-MII 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C900TPO	0x9000		/* 3c900-TPO Ethernet */
#define	PCI_PRODUCT_3COM_3C900COMBO	0x9001		/* 3c900-COMBO Ethernet */
#define	PCI_PRODUCT_3COM_3C905TX	0x9050		/* 3c905-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905T4	0x9051		/* 3c905-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C900BTPO	0x9004		/* 3c900B-TPO Ethernet */
#define	PCI_PRODUCT_3COM_3C900BCOMBO	0x9005		/* 3c900B-COMBO Ethernet */
#define	PCI_PRODUCT_3COM_3C900BTPC	0x9006		/* 3c900B-TPC Ethernet */
#define	PCI_PRODUCT_3COM_3C905BTX	0x9055		/* 3c905B-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BT4	0x9056		/* 3c905B-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BCOMBO	0x9058		/* 3c905B-COMBO 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BFX	0x905a		/* 3c905B-FX 100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905CTX	0x9200		/* 3c905C-TX 10/100 Ethernet with mngmt */
#define	PCI_PRODUCT_3COM_3C980SRV	0x9800		/* 3c980 Server Adapter 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C980CTXM	0x9805		/* 3c980C-TXM 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3CR990TX97	0x9903		/* 3CR990-TX-97 10/100 Ethernet */

/* 3Dfx Interactive producs */
#define	PCI_PRODUCT_3DFX_VOODOO	0x0001		/* Voodoo */
#define	PCI_PRODUCT_3DFX_VOODOO2	0x0002		/* Voodoo2 */
#define	PCI_PRODUCT_3DFX_BANSHEE	0x0003		/* Banshee */
#define	PCI_PRODUCT_3DFX_VOODOO3	0x0005		/* Voodoo3 */

/* 3D Labs products */
#define	PCI_PRODUCT_3DLABS_300SX	0x0001		/* GLINT 300SX */
#define	PCI_PRODUCT_3DLABS_500TX	0x0002		/* GLINT 500TX */
#define	PCI_PRODUCT_3DLABS_DELTA	0x0003		/* GLINT DELTA */
#define	PCI_PRODUCT_3DLABS_PERMEDIA	0x0004		/* GLINT Permedia */
#define	PCI_PRODUCT_3DLABS_500MX	0x0006		/* GLINT 500MX */
#define	PCI_PRODUCT_3DLABS_PERMEDI2	0x0007		/* GLINT Permedia 2 */

/* 3ware products */
#define	PCI_PRODUCT_3WARE_ESCALADE	0x1000		/* Escalade IDE RAID */

/* ACC Products */
#define	PCI_PRODUCT_ACC_2188	0x0000		/* ACCM 2188 VL-PCI Bridge */
#define	PCI_PRODUCT_ACC_2051_HB	0x2051		/* 2051 PCI Single Chip Solution (host bridge) */
#define	PCI_PRODUCT_ACC_2051_ISA	0x5842		/* 2051 PCI Single Chip Solution (ISA bridge) */

/* Acard products */
#define	PCI_PRODUCT_ACARD_AEC6710	0x8002		/* AEC6710 SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712UW	0x8010		/* AEC6712UW SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712U	0x8020		/* AEC6712U SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712S	0x8030		/* AEC6712S SCSI */
#define	PCI_PRODUCT_ACARD_AEC6710D	0x8040		/* AEC6710D SCSI */
#define	PCI_PRODUCT_ACARD_AEC6715UW	0x8050		/* AEC6715UW SCSI */

/* Accton products */
#define	PCI_PRODUCT_ACCTON_MPX5030	0x1211		/* MPX 5030/5038 Ethernet */

/* Acer products */
#define	PCI_PRODUCT_ACER_M1435	0x1435		/* M1435 VL-PCI Bridge */

/* Acer Labs products */
#define	PCI_PRODUCT_ALI_M1445	0x1445		/* M1445 VL-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1449	0x1449		/* M1449 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1451	0x1451		/* M1451 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1461	0x1461		/* M1461 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1531	0x1531		/* M1531 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1541	0x1541		/* M1541 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1543	0x1533		/* M1543 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M3309	0x3309		/* M3309 MPEG Decoder */
#define	PCI_PRODUCT_ALI_M4803	0x5215		/* M4803 */
#define	PCI_PRODUCT_ALI_M5229	0x5229		/* M5229 UDMA IDE Controller */
#define	PCI_PRODUCT_ALI_M5237	0x5237		/* M5237 USB Host Controller */
#define	PCI_PRODUCT_ALI_M7101	0x7101		/* M7101 Power Management Controller */

/* Adaptec products */
#define	PCI_PRODUCT_ADP_AIC7850	0x5078		/* AIC-7850 */
#define	PCI_PRODUCT_ADP_AIC7855	0x5578		/* AIC-7855 */
#define	PCI_PRODUCT_ADP_AIC5900	0x5900		/* AIC-5900 ATM */
#define	PCI_PRODUCT_ADP_AIC5905	0x5905		/* AIC-5905 ATM */
#define	PCI_PRODUCT_ADP_AIC6915	0x6915		/* AIC-6915 10/100 Ethernet */
#define	PCI_PRODUCT_ADP_AIC7860	0x6078		/* AIC-7860 */
#define	PCI_PRODUCT_ADP_APA1480	0x6075		/* APA-1480 Ultra */
#define	PCI_PRODUCT_ADP_2940AU	0x6178		/* AHA-2940A Ultra */
#define	PCI_PRODUCT_ADP_AIC7870	0x7078		/* AIC-7870 */
#define	PCI_PRODUCT_ADP_2940	0x7178		/* AHA-2940 */
#define	PCI_PRODUCT_ADP_3940	0x7278		/* AHA-3940 */
#define	PCI_PRODUCT_ADP_3985	0x7378		/* AHA-3985 */
#define	PCI_PRODUCT_ADP_2944	0x7478		/* AHA-2944 */
#define	PCI_PRODUCT_ADP_AIC7895	0x7895		/* AIC-7895 Ultra */
#define	PCI_PRODUCT_ADP_AIC7880	0x8078		/* AIC-7880 Ultra */
#define	PCI_PRODUCT_ADP_2940U	0x8178		/* AHA-2940 Ultra */
#define	PCI_PRODUCT_ADP_3940U	0x8278		/* AHA-3940 Ultra */
#define	PCI_PRODUCT_ADP_389XU	0x8378		/* AHA-389X Ultra */
#define	PCI_PRODUCT_ADP_2944U	0x8478		/* AHA-2944 Ultra */
#define	PCI_PRODUCT_ADP_2940UP	0x8778		/* AHA-2940 Ultra Pro */

#define	PCI_PRODUCT_ADP2_2940U2	0x0010		/* AHA-2940 Ultra2 */
#define	PCI_PRODUCT_ADP2_2930U2	0x0011		/* AHA-2930 Ultra2 */
#define	PCI_PRODUCT_ADP2_AIC7890	0x001f		/* AIC-7890/1 */
#define	PCI_PRODUCT_ADP2_3950U2B	0x0050		/* AHA-3950 Ultra2 */
#define	PCI_PRODUCT_ADP2_3950U2D	0x0051		/* AHA-3950 Ultra2 */
#define	PCI_PRODUCT_ADP2_AIC7896	0x005f		/* AIC-7896/7 */

/* Addtron Products */
#define	PCI_PRODUCT_ADDTRON_8139	0x1360		/* 8139 Ethernet */

/* ADMtek products */
#define	PCI_PRODUCT_ADMTEK_AL981	0x0981		/* ADMtek AL981 10/100 Ethernet */

/* Advanced System Products */
#define	PCI_PRODUCT_ADVSYS_1200A	0x1100	
#define	PCI_PRODUCT_ADVSYS_1200B	0x1200	
#define	PCI_PRODUCT_ADVSYS_ULTRA	0x1300		/* ABP-930/40UA */
#define	PCI_PRODUCT_ADVSYS_WIDE	0x2300		/* ABP-940UW */
#define	PCI_PRODUCT_ADVSYS_U2W	0x2500		/* ASB-3940U2W */
#define	PCI_PRODUCT_ADVSYS_U3W	0x2700		/* ASB-3940U3W */

/* Alliance products */
#define	PCI_PRODUCT_ALLIANCE_AT24	0x6424		/* AT24 */
#define	PCI_PRODUCT_ALLIANCE_AT25	0x643d		/* AT25 */

/* Alteon products */
#define	PCI_PRODUCT_ALTEON_ACENIC	0x0001		/* ACEnic Gigabit Ethernet */

/* AMD products */
#define	PCI_PRODUCT_AMD_PCNET_PCI	0x2000		/* 79c970 PCnet-PCI LANCE Ethernet */
#define	PCI_PRODUCT_AMD_PCNET_HOME	0x2001		/* 79c978 PCnet-PCI Home */
#define	PCI_PRODUCT_AMD_PCSCSI_PCI	0x2020		/* 53c974 PCscsi-PCI SCSI */
#define	PCI_PRODUCT_AMD_PCNETS_PCI	0x2040		/* 79C974 PCnet-PCI Ethernet & SCSI */
#define	PCI_PRODUCT_AMD_SC751_SC	0x7006		/* AMD751 System Controller */
#define	PCI_PRODUCT_AMD_SC751_PPB	0x7007		/* AMD751 PCI-to-PCI Bridge */
#define	PCI_PRODUCT_AMD_PBC756_ISA	0x7408		/* AMD756 PCI-to-ISA Bridge */
#define	PCI_PRODUCT_AMD_PBC756_IDE	0x7409		/* AMD756 IDE controller */
#define	PCI_PRODUCT_AMD_PBC756_PMC	0x740B		/* AMD756 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC756_USB	0x740C		/* AMD756 USB Host Controller */
#define	PCI_PRODUCT_AMD_HT7520	0x7450		/* (PLX) HT7520 PCIX Tunnel */
#define	PCI_PRODUCT_AMD_HT7520_PIC	0x7451		/* (PLX) HT7520 PCIX IOAPIC */
#define	PCI_PRODUCT_AMD_AMD8151_AGP	0x7454		/* AMD8151 AGP Device */
#define	PCI_PRODUCT_AMD_AMD8151	0x7455		/* AMD8151 HyperTransport-AGP Bridge */

/* API Networks products */
#define	PCI_PRODUCT_API_STURGEON	0x0010		/* AP1011 HyperTransport-PCI Bridge */

/* Apple products */
#define	PCI_PRODUCT_APPLE_BANDIT	0x0001		/* Bandit Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_GC	0x0002		/* Grand Central I/O Controller */
#define	PCI_PRODUCT_APPLE_CONTROL	0x0003		/* Control */
#define	PCI_PRODUCT_APPLE_PLANB	0x0004		/* PlanB */
#define	PCI_PRODUCT_APPLE_OHARE	0x0007		/* OHare I/O Controller */
#define	PCI_PRODUCT_APPLE_BANDIT2	0x0008		/* Bandit Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_HEATHROW	0x0010		/* MAC-IO I/O Controller (Heathrow) */
#define	PCI_PRODUCT_APPLE_PADDINGTON	0x0017		/* MAC-IO I/O Controller (Paddington) */
#define	PCI_PRODUCT_APPLE_KEYLARGO_USB	0x0019		/* KeyLargo USB Controller */
#define	PCI_PRODUCT_APPLE_UNINORTH1	0x001e		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH2	0x001f		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP	0x0020		/* UniNorth AGP Interface */
#define	PCI_PRODUCT_APPLE_GMAC	0x0021		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_KEYLARGO	0x0022		/* MAC-IO I/O Controller (KeyLargo) */

/* ARC Logic products */
#define	PCI_PRODUCT_ARC_1000PV	0xa091		/* 1000PV */
#define	PCI_PRODUCT_ARC_2000PV	0xa099		/* 2000PV */
#define	PCI_PRODUCT_ARC_2000MT	0xa0a1		/* 2000MT */

/* ASIX Electronics products */
#define	PCI_PRODUCT_ASIX_AX88140A	0x1400		/* AX88140A 10/100 Ethernet */

/* ATI products */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_MACH64_CT	0x4354		/* Mach64 CT */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64 CX */
#define	PCI_PRODUCT_ATI_MACH64_ET	0x4554		/* Mach64 ET */
#define	PCI_PRODUCT_ATI_MACH64_VT	0x4654		/* Mach64 VT */
#define	PCI_PRODUCT_ATI_MACH64_B	0x4750		/* Mach64 B */
#define	PCI_PRODUCT_ATI_MACH64_GB	0x4742		/* Mach64 GB */
#define	PCI_PRODUCT_ATI_MACH64_GD	0x4744		/* Mach64 GD */
#define	PCI_PRODUCT_ATI_MACH64_GI	0x4749		/* Mach64 GI */
#define	PCI_PRODUCT_ATI_MACH64_GP	0x4750		/* Mach64 GP */
#define	PCI_PRODUCT_ATI_MACH64_GQ	0x4751		/* Mach64 GQ */
#define	PCI_PRODUCT_ATI_MACH64_GT	0x4754		/* Mach64 GT */
#define	PCI_PRODUCT_ATI_MACH64_GU	0x4755		/* Mach64 GU */
#define	PCI_PRODUCT_ATI_MACH64_GV	0x4756		/* Mach64 GV */
#define	PCI_PRODUCT_ATI_MACH64_GW	0x4757		/* Mach64 GW */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64 GX */
#define	PCI_PRODUCT_ATI_MACH64_GZ	0x475a		/* Mach64 GZ */
#define	PCI_PRODUCT_ATI_MACH64_LB	0x4c42		/* Mach64 LB */
#define	PCI_PRODUCT_ATI_MACH64_LD	0x4c44		/* Mach64 LD */
#define	PCI_PRODUCT_ATI_MACH64_LG	0x4c47		/* Mach64 LG */
#define	PCI_PRODUCT_ATI_MACH64_LI	0x4c49		/* Mach64 LI */
#define	PCI_PRODUCT_ATI_MACH64_LM	0x4c4d		/* Mach64 LM */
#define	PCI_PRODUCT_ATI_MACH64_LP	0x4c50		/* Mach64 LP */
#define	PCI_PRODUCT_ATI_MACH64_LR	0x4c52		/* Mach64 LR */

/* Auravision products */
#define	PCI_PRODUCT_AURAVISION_VXP524	0x01f7		/* VxP524 PCI Video Processor */

/* Aureal Semiconductor */
#define	PCI_PRODUCT_AUREAL_AU8820	0x0001		/* AU8820 Vortex Digital Audio Processor */

/* Applied Micro Circuts products */
#define	PCI_PRODUCT_AMCIRCUITS_S5933	0x4750		/* S5933 PCI Matchmaker */
#define	PCI_PRODUCT_AMCIRCUITS_LANAI	0x8043		/* Myrinet LANai Interface */
#define	PCI_PRODUCT_AMCIRCUITS_S5920	0x5920		/* S5920 PCI Target */

/* Atronics products */
#define	PCI_PRODUCT_ATRONICS_IDE_2015PL	0x2015		/* IDE-2015PL */

/* Avance Logic products */
#define	PCI_PRODUCT_AVANCE_AVL2301	0x2301		/* AVL2301 */
#define	PCI_PRODUCT_AVANCE_AVG2302	0x2302		/* AVG2302 */
#define	PCI_PRODUCT_AVANCE2_ALG2301	0x2301		/* ALG2301 */
#define	PCI_PRODUCT_AVANCE2_ALG2302	0x2302		/* ALG2302 */

/* CCUBE products */
#define	PCI_PRODUCT_CCUBE_CINEMASTER	0x8888		/* Cinemaster C 3.0 DVD Decoder */

/* AVM products */
#define	PCI_PRODUCT_AVM_FRITZ_CARD	0x0a00		/* Fritz! Card ISDN Interface */

/* Bit3 products */
#define	PCI_PRODUCT_BIT3_PCIVME617	0x0001		/* PCI-VME Interface Mod. 617 */
#define	PCI_PRODUCT_BIT3_PCIVME618	0x0010		/* PCI-VME Interface Mod. 618 */
#define	PCI_PRODUCT_BIT3_PCIVME2706	0x0300		/* PCI-VME Interface Mod. 2706 */

/* Broadcom products */
#define	PCI_PRODUCT_BROADCOM_BCM4211	0x4211		/* BCM4211 iLine10 Controller */
#define	PCI_PRODUCT_BROADCOM_BCM4212	0x4212		/* BCM4212 V.90 Modem */
#define	PCI_PRODUCT_BROADCOM_BCM5700	0x1644		/* BCM5700 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5701	0x1645		/* BCM5701 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5750	0x1676		/* BCM5750 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702	0x16a6		/* BCM5702 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703	0x16a7		/* BCM5703 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705	0x1653		/* BCM5705 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM4704	0x4704		/* BCM4704 PCI Host Bridge */
#define	PCI_PRODUCT_BROADCOM_BCM4710	0x4710		/* BCM4710/4702 PCI Host Bridge */
#define	PCI_PRODUCT_BROADCOM_BCM471F	0x471f		/* BCM4785 10/100/1000 Ethernet Core */
#define	PCI_PRODUCT_BROADCOM_BCM5820	0x5820		/* BCM5820 eCommerce Processor */
#define	PCI_PRODUCT_BROADCOM_BCM5821	0x5821		/* BCM5821 Super-eCommerce Processor */
#define	PCI_PRODUCT_BROADCOM_BCM5850	0x5850		/* BCM5850 SSL/TLS Protocol Processor */

/* Brooktree products */
#define	PCI_PRODUCT_BROOKTREE_BT848	0x0350		/* Bt848 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT849	0x0351		/* Bt849 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT878	0x036e		/* Bt878 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT879	0x036f		/* Bt879 Video Capture */

/* BusLogic products */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC	0x0140		/* MultiMaster NC */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER	0x1040		/* MultiMaster */
#define	PCI_PRODUCT_BUSLOGIC_FLASHPOINT	0x8130		/* FlashPoint */

/* c't Magazin products */
#define	PCI_PRODUCT_C4T_GPPCI	0x6773		/* GPPCI */

/* Chips and Technologies products */
#define	PCI_PRODUCT_CHIPS_64310	0x00b8		/* 64310 */
#define	PCI_PRODUCT_CHIPS_65545	0x00d8		/* 65545 */
#define	PCI_PRODUCT_CHIPS_65548	0x00dc		/* 65548 */
#define	PCI_PRODUCT_CHIPS_65550	0x00e0		/* 65550 */
#define	PCI_PRODUCT_CHIPS_65554	0x00e4		/* 65554 */

/* Cirrus Logic products */
#define	PCI_PRODUCT_CIRRUS_CL_GD7548	0x0038		/* CL-GD7548 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5430	0x00a0		/* CL-GD5430 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_4	0x00a4		/* CL-GD5434-4 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_8	0x00a8		/* CL-GD5434-8 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5436	0x00ac		/* CL-GD5436 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5446	0x00b8		/* CL-GD5446 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5480	0x00bc		/* CL-GD5480 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6729	0x1100		/* CL-PD6729 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6832	0x1110		/* CL-PD6832 PCI-CardBus Bridge */
#define	PCI_PRODUCT_CIRRUS_CL_PD6833	0x1113		/* CL-PD6833 PCI-CardBus Bridge */
#define	PCI_PRODUCT_CIRRUS_CL_GD7542	0x1200		/* CL-GD7542 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7543	0x1202		/* CL-GD7543 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7541	0x1204		/* CL-GD7541 */
#define	PCI_PRODUCT_CIRRUS_CL_CD4400	0x4400		/* CL-CD4400 Communications Controller */
#define	PCI_PRODUCT_CIRRUS_CS4610	0x6001		/* CS4610 SoundFusion Audio Accelerator */
#define	PCI_PRODUCT_CIRRUS_CS4280	0x6003		/* CS4280 CrystalClear Audio Interface */

/* CMD Technology products -- info gleaned from their web site */
#define	PCI_PRODUCT_CMDTECH_640	0x0640		/* PCI0640 */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_642	0x0642		/* PCI0642 */
/* datasheets available from www.cmd.com for the followings */
#define	PCI_PRODUCT_CMDTECH_643	0x0643		/* PCI0643 */
#define	PCI_PRODUCT_CMDTECH_646	0x0646		/* PCI0646 */
#define	PCI_PRODUCT_CMDTECH_647	0x0647		/* PCI0647 */
#define	PCI_PRODUCT_CMDTECH_648	0x0648		/* PCI0648 */
#define	PCI_PRODUCT_CMDTECH_649	0x0649		/* PCI0649 */

/* Inclusion of 'A' in the following entry is probably wrong. */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_650A	0x0650		/* PCI0650A */
#define	PCI_PRODUCT_CMDTECH_670	0x0670		/* USB0670 */
#define	PCI_PRODUCT_CMDTECH_673	0x0673		/* USB0673 */

/* C-Media products */
#define	PCI_PRODUCT_CMEDIA_CMI8338A	0x0100		/* CMI8338A PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8338B	0x0101		/* CMI8338B PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8738	0x0111		/* CMI8738/C3DX PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_HSP56	0x0211		/* HSP56 Audiomodem Riser */

/* Cogent Data Technologies products */
#define	PCI_PRODUCT_COGENT_EM110TX	0x1400		/* EX110TX PCI Fast Ethernet Adapter */

/* Compaq products */
#define	PCI_PRODUCT_COMPAQ_PCI_EISA_BRIDGE	0x0001		/* PCI-EISA Bridge */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE	0x0002		/* PCI-ISA Bridge */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX1	0x1000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX2	0x2000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_QVISION_V0	0x3032		/* QVision */
#define	PCI_PRODUCT_COMPAQ_QVISION_1280P	0x3033		/* QVision 1280/p */
#define	PCI_PRODUCT_COMPAQ_QVISION_V2	0x3034		/* QVision */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX4	0x4000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_USB	0x7020		/* USB Controller */
#define	PCI_PRODUCT_COMPAQ_SMART2P	0xae10		/* SMART2P RAID */
#define	PCI_PRODUCT_COMPAQ_N100TX	0xae32		/* Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_N10T	0xae34		/* Netelligent 10 T */
#define	PCI_PRODUCT_COMPAQ_IntNF3P	0xae35		/* Integrated NetFlex 3/P */
#define	PCI_PRODUCT_COMPAQ_DPNet100TX	0xae40		/* Dual Port Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_IntPL100TX	0xae43		/* ProLiant Integrated Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_DP4000	0xb011		/* Deskpro 4000 5233MMX */
#define	PCI_PRODUCT_COMPAQ_NF3P_BNC	0xf150		/* NetFlex 3/P w/ BNC */
#define	PCI_PRODUCT_COMPAQ_NF3P	0xf130		/* NetFlex 3/P */

#define	PCI_PRODUCT_COMPEX_NE2KETHER	0x1401		/* Ethernet */
#define	PCI_PRODUCT_COMPEX_RL100ATX	0x2011		/* RL100-ATX 10/100 Ethernet */
#define	PCI_PRODUCT_COMPEX_RL100TX	0x9881		/* RL100-TX 10/100 Ethernet */

/* Conexant Systems products */
#define	PCI_PRODUCT_CONEXANT_SOFTK56	0x2443		/* SoftK56 PCI Software Modem */

/* Contaq Microsystems products */
#define	PCI_PRODUCT_CONTAQ_82C599	0x0600		/* 82C599 PCI-VLB Bridge */
#define	PCI_PRODUCT_CONTAQ_82C693	0xc693		/* 82C693 PCI-ISA Bridge */

/* Corollary Products */
#define	PCI_PRODUCT_COROLLARY_CBUSII_PCIB	0x0014		/* \"C-Bus II\"-PCI Bridge */

/* Creative Labs products */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE	0x0002		/* SBLive! EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY	0x7002		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_EV1938	0x8938		/* Ectiva 1938 */

/* Cyclades products */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_1	0x0100		/* Cyclom-Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_2	0x0101		/* Cyclom-Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_1	0x0102		/* Cyclom-4Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_2	0x0103		/* Cyclom-4Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_1	0x0104		/* Cyclom-8Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_2	0x0105		/* Cyclom-8Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_1	0x0200		/* Cyclom-Z below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_2	0x0201		/* Cyclom-Z above 1M */

/* Davicom Semiconductor products */
#define	PCI_PRODUCT_DAVICOM_DM9102	0x9102		/* Davicom DM9102 10/100 Ethernet */

/* DEC products */
#define	PCI_PRODUCT_DEC_21050	0x0001		/* DECchip 21050 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21040	0x0002		/* DECchip 21040 Ethernet */
#define	PCI_PRODUCT_DEC_21030	0x0004		/* DECchip 21030 (\"TGA\") */
#define	PCI_PRODUCT_DEC_NVRAM	0x0007		/* Zephyr NV-RAM */
#define	PCI_PRODUCT_DEC_KZPSA	0x0008		/* KZPSA */
#define	PCI_PRODUCT_DEC_21140	0x0009		/* DECchip 21140 10/100 Ethernet */
#define	PCI_PRODUCT_DEC_PBXGB	0x000d		/* TGA2 */
#define	PCI_PRODUCT_DEC_DEFPA	0x000f		/* DEFPA */
/* product DEC ???	0x0010	??? VME Interface */
#define	PCI_PRODUCT_DEC_21041	0x0014		/* DECchip 21041 Ethernet */
#define	PCI_PRODUCT_DEC_DGLPB	0x0016		/* DGLPB (\"OPPO\") */
#define	PCI_PRODUCT_DEC_21142	0x0019		/* DECchip 21142/21143 10/100 Ethernet */
#define	PCI_PRODUCT_DEC_21052	0x0021		/* DECchip 21052 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21150	0x0022		/* DECchip 21150 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21152	0x0024		/* DECchip 21152 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21153	0x0025		/* DECchip 21153 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21154	0x0026		/* DECchip 21154 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_CPQ42XX	0x0046		/* Compaq SMART RAID 42xx */

/* Delta products */
#define	PCI_PRODUCT_DELTA_8139	0x1360		/* 8139 Ethernet */

/* Diamond products */
#define	PCI_PRODUCT_DIAMOND_VIPER	0x9001		/* Viper/PCI */

/* D-Link Systems products */
#define	PCI_PRODUCT_DLINK_DFE550TX	0x1002		/* DFE-550TX 10/100 Ethernet */

/* Distributed Processing Technology products */
#define	PCI_PRODUCT_DPT_SC_RAID	0xa400		/* SmartCache/SmartRAID */
#define	PCI_PRODUCT_DPT_RAID_I2O	0xa501		/* SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_MEMCTLR	0x1012		/* Memory Controller */

/* Dolphin products */
#define	PCI_PRODUCT_DOLPHIN_PCISCI	0x0658		/* PCI-SCI Bridge */

/* Domex products */
#define	PCI_PRODUCT_DOMEX_PCISCSI	0x0001		/* DMX-3191D */

/* ELSA products */
#define	PCI_PRODUCT_ELSA_QS1PCI	0x1000		/* QuickStep 1000 ISDN card */

/* Emulex products */
#define	PCI_PRODUCT_EMULEX_LPPFC	0x10df		/* \"Light Pulse\" FibreChannel adapter */

/* Ensoniq products */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI	0x5000		/* AudioPCI */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI97	0x1371		/* AudioPCI 97 */
#define	PCI_PRODUCT_ENSONIQ_CT5880	0x5880		/* CT5880 */

/* Epigram (now Broadcom) products */
#define	PCI_PRODUCT_EPIGRAM_BCM4210	0xa0fa		/* BCM4210 iLine10 Controller */

/* Essential Communications products */
#define	PCI_PRODUCT_ESSENTIAL_RR_HIPPI	0x0001		/* RoadRunner HIPPI Interface */
#define	PCI_PRODUCT_ESSENTIAL_RR_GIGE	0x0005		/* RoadRunner Gig-E Interface */

/* ESS Technology Inc products */
#define	PCI_PRODUCT_ESSTECH_MAESTRO1	0x0100		/* Maestro 1 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2	0x1968		/* Maestro 2 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_SOLO1	0x1969		/* Solo-1 PCI AudioDrive */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2E	0x1978		/* Maestro 2E PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3	0x1998		/* Maestro 3 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3MODEM	0x1999		/* Maestro 3 Modem */

/* ESS Technology Inc products */
#define	PCI_PRODUCT_ESSTECH2_MAESTRO1	0x0100		/* Maestro 1 PCI Audio Accelerator */

/* O2 Micro Inc */
#define	PCI_PRODUCT_O2MICRO_OZ6832	0x6832		/* OZ6832 CardBus Controller */

/* Evans & Sutherland products */
#define	PCI_PRODUCT_ES_FREEDOM	0x0001		/* Freedom PCI-GBus Interface */

/* FORE products */
#define	PCI_PRODUCT_FORE_PCA200	0x0210		/* ATM PCA-200 */
#define	PCI_PRODUCT_FORE_PCA200E	0x0300		/* ATM PCA-200e */

/* Forte Media products */
#define	PCI_PRODUCT_FORTEMEDIA_FM801	0x0801		/* Forte Media 801 Sound */

/* Future Domain products */
#define	PCI_PRODUCT_FUTUREDOMAIN_TMC_18C30	0x0000		/* TMC-18C30 (36C70) */

/* Efficient Networks products */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PF	0x0000		/* 155P-MF1 ATM (FPGA) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PA	0x0002		/* 155P-MF1 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI25P	0x0003		/* SpeedStream ENI-25p */
#define	PCI_PRODUCT_EFFICIENTNETS_SS3000	0x0005		/* SpeedStream 3000 */

/* Galileo Technology products */
#define	PCI_PRODUCT_GALILEO_GT64010A	0x0146		/* GT-64010A System Controller */
#define	PCI_PRODUCT_GALILEO_GT64115	0x4111		/* GT-64115 System Controller */
#define	PCI_PRODUCT_GALILEO_GT64011	0x4146		/* GT-64011 System Controller */
#define	PCI_PRODUCT_GALILEO_GT64120	0x4620		/* GT-64120 System Controller */
#define	PCI_PRODUCT_GALILEO_GT64130	0x6320		/* GT-64130 System Controller */

/* Hewlett-Packard products */
#define	PCI_PRODUCT_HP_J2585A	0x1030		/* J2585A */

/* IBM products */
#define	PCI_PRODUCT_IBM_MCABRIDGE	0x0002		/* MCA Bridge */
#define	PCI_PRODUCT_IBM_ALTALITE	0x0005		/* CPU Bridge - Alta Lite */
#define	PCI_PRODUCT_IBM_ALTAMP	0x0007		/* CPU Bridge - Alta MP */
#define	PCI_PRODUCT_IBM_ISABRIDGE	0x000a		/* ISA Bridge w/PnP */
#define	PCI_PRODUCT_IBM_CPUBRIDGE	0x0017		/* CPU Bridge */
#define	PCI_PRODUCT_IBM_LANSTREAMER	0x0018		/* Auto LANStreamer */
#define	PCI_PRODUCT_IBM_GXT150P	0x001b		/* GXT-150P 2D Accelerator */
#define	PCI_PRODUCT_IBM_MCABRIDGE2	0x0020		/* MCA Bridge */
#define	PCI_PRODUCT_IBM_82351	0x0022		/* 82351 PCI-PCI Bridge */
#define	PCI_PRODUCT_IBM_SERVERAID	0x002e		/* ServeRAID */
#define	PCI_PRODUCT_IBM_OLYMPIC	0x003e		/* Token Ring */
#define	PCI_PRODUCT_IBM_MIAMI	0x0036		/* Miami/PCI */
#define	PCI_PRODUCT_IBM_TURBOWAYS25	0x0053		/* Turboways 25 ATM */
#define	PCI_PRODUCT_IBM_MPIC2	0xffff		/* MPIC-II */

/* IDT products */
#define	PCI_PRODUCT_IDT_77201	0x0001		/* 77201/77211 ATM (\"NICStAR\") */

/* Initio products */
#define	PCI_PRODUCT_INITIO_I920	0x0002		/* INIC-920 SCSI */
#define	PCI_PRODUCT_INITIO_I940	0x9400		/* INIC-940 SCSI */
#define	PCI_PRODUCT_INITIO_I935	0x9401		/* INIC-935 SCSI */
#define	PCI_PRODUCT_INITIO_I950	0x9500		/* INIC-950 SCSI */

/* Integrated Micro Solutions products */
#define	PCI_PRODUCT_IMS_8849	0x8849		/* 8849 */
#define	PCI_PRODUCT_IMS_TT128M	0x9128		/* TwinTurbo 128M */

/* Intel products */
#define	PCI_PRODUCT_INTEL_PCEB	0x0482		/* 82375EB/SB PCI-EISA Bridge (PCEB) */
#define	PCI_PRODUCT_INTEL_CDC	0x0483		/* 82424ZX Cache and DRAM controller (CDC) */
#define	PCI_PRODUCT_INTEL_SIO	0x0484		/* 82378ZB System I/O (SIO) */
#define	PCI_PRODUCT_INTEL_82426EX	0x0486		/* 82426EX PCI-to-ISA Bridge (PCIB) */
#define	PCI_PRODUCT_INTEL_PCMC	0x04a3		/* 82434LX/NX PCI, Cache and Memory Controller (PCMC) */
#define	PCI_PRODUCT_INTEL_IN_BUSINESS	0x1030		/* InBusiness Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82559ER	0x1209		/* 82559ER Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82092AA	0x1222		/* 82092AA IDE controller */
#define	PCI_PRODUCT_INTEL_SAA7116	0x1223		/* SAA7116 */
#define	PCI_PRODUCT_INTEL_82596	0x1226		/* 82596 LAN Controller */
#define	PCI_PRODUCT_INTEL_EEPRO100	0x1227		/* EE Pro 100 10/100 Fast Ethernet */
#define	PCI_PRODUCT_INTEL_EEPRO100S	0x1228		/* EE Pro 100 Smart 10/100 Fast Ethernet */
#define	PCI_PRODUCT_INTEL_82557	0x1229		/* 82557 Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82437FX	0x122d		/* 82437FX System Controller (TSC) */
#define	PCI_PRODUCT_INTEL_82371FB_ISA	0x122e		/* 82371FB PCI-to-ISA Bridge (PIIX) */
#define	PCI_PRODUCT_INTEL_82371FB_IDE	0x1230		/* 82371FB IDE controller (PIIX) */
#define	PCI_PRODUCT_INTEL_82371MX	0x1234		/* 82371MX Mobile PCI I/O IDE Xcelerator (MPIIX) */
#define	PCI_PRODUCT_INTEL_82437MX	0x1235		/* 82437MX Mobile System Controller (MTSC) */
#define	PCI_PRODUCT_INTEL_82441FX	0x1237		/* 82441FX PCI and Memory Controller (PMC) */
#define	PCI_PRODUCT_INTEL_82380AB	0x123c		/* 82380AB Mobile PCI-to-ISA Bridge (MISA) */
#define	PCI_PRODUCT_INTEL_82380FB	0x124b		/* 82380FB Mobile PCI-to-PCI Bridge (MPCI2) */
#define	PCI_PRODUCT_INTEL_82439HX	0x1250		/* 82439HX System Controller (TXC) */
#define	PCI_PRODUCT_INTEL_82801AA_LPC	0x2410		/* 82801AA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801AA_IDE	0x2411		/* 82801AA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801AA_USB	0x2412		/* 82801AA USB Controller */
#define	PCI_PRODUCT_INTEL_82801AA_SMB	0x2413		/* 82801AA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801AA_ACA	0x2415		/* 82801AA AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801AA_ACM	0x2416		/* 82801AA AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801AA_HPB	0x2418		/* 82801AA Hub-to-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801AB_LPC	0x2420		/* 82801AB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801AB_IDE	0x2421		/* 82801AB IDE Controller */
#define	PCI_PRODUCT_INTEL_82801AB_USB	0x2422		/* 82801AB USB Controller */
#define	PCI_PRODUCT_INTEL_82801AB_SMB	0x2423		/* 82801AB SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801AB_ACA	0x2425		/* 82801AB AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801AB_ACM	0x2426		/* 82801AB AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801AB_HPB	0x2428		/* 82801AB Hub-to-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_LPC	0x2440		/* 82801BA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_USB1	0x2442		/* 82801BA USB Controller */
#define	PCI_PRODUCT_INTEL_82801BA_SMB	0x2443		/* 82801BA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801BA_USB2	0x2444		/* 82801BA USB Controller */
#define	PCI_PRODUCT_INTEL_82801BA_ACA	0x2445		/* 82801BA AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801BA_ACM	0x2446		/* 82801BA AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801BA_LAN	0x2449		/* 82801BA LAN Controller */
#define	PCI_PRODUCT_INTEL_82801BA_IDE	0x244B		/* 82801BA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801BA_HPB	0x244E		/* 82801BA Hub-to-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82371SB_ISA	0x7000		/* 82371SB PCI-to-ISA Bridge (PIIX3) */
#define	PCI_PRODUCT_INTEL_82371SB_IDE	0x7010		/* 82371SB IDE Interface (PIIX3) */
#define	PCI_PRODUCT_INTEL_82371SB_USB	0x7020		/* 82371SB USB Host Controller (PIIX3) */
#define	PCI_PRODUCT_INTEL_82437VX	0x7030		/* 82437VX System Controller (TVX) */
#define	PCI_PRODUCT_INTEL_82439TX	0x7100		/* 82439TX System Controller (MTXC) */
#define	PCI_PRODUCT_INTEL_82371AB_ISA	0x7110		/* 82371AB PCI-to-ISA Bridge (PIIX4) */
#define	PCI_PRODUCT_INTEL_82371AB_IDE	0x7111		/* 82371AB IDE controller (PIIX4) */
#define	PCI_PRODUCT_INTEL_82371AB_USB	0x7112		/* 82371AB USB Host Controller (PIIX4) */
#define	PCI_PRODUCT_INTEL_82371AB_PMC	0x7113		/* 82371AB Power Management Controller (PIIX4) */
#define	PCI_PRODUCT_INTEL_82810_MCH	0x7120		/* 82810 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810_GC	0x7121		/* 82810 Graphics Controller */
#define	PCI_PRODUCT_INTEL_82810_DC100_MCH	0x7122		/* 82810-DC100 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810_DC100_GC	0x7123		/* 82810-DC100 Graphics Controller */
#define	PCI_PRODUCT_INTEL_82810E_MCH	0x7124		/* 82810E Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810E_GC	0x7125		/* 82810E Graphics Controller */
#define	PCI_PRODUCT_INTEL_82443LX	0x7180		/* 82443LX PCI AGP Controller (PAC) */
#define	PCI_PRODUCT_INTEL_82443LX_AGP	0x7181		/* 82443LX AGP Interface (PAC) */
#define	PCI_PRODUCT_INTEL_82443BX	0x7190		/* 82443BX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82443BX_AGP	0x7191		/* 82443BX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443BX_NOAGP	0x7192		/* 82443BX Host Bridge/Controller (AGP disabled) */
#define	PCI_PRODUCT_INTEL_82440MX	0x7194		/* 82440MX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82440MX_ACA	0x7195		/* 82440MX AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82440MX_ISA	0x7198		/* 82440MX PCI-to-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82440MX_IDE	0x7199		/* 82440MX IDE Controller */
#define	PCI_PRODUCT_INTEL_82440MX_USB	0x719a		/* 82440MX USB Host Controller */
#define	PCI_PRODUCT_INTEL_82440MX_PMC	0x719b		/* 82440MX Power Management Controller */
#define	PCI_PRODUCT_INTEL_I740	0x7800		/* i740 Graphics Accelerator */
#define	PCI_PRODUCT_INTEL_PCI450_PB	0x84c4		/* 82454KX/GX PCI Bridge (PB) */
#define	PCI_PRODUCT_INTEL_PCI450_MC	0x84c5		/* 82451KX/GX Memory Controller (MC) */
#define	PCI_PRODUCT_INTEL_82451NX_MIOC	0x84ca		/* 82451NX Memory & I/O Controller (MIOC) */
#define	PCI_PRODUCT_INTEL_82451NX_PXB	0x84cb		/* 82451NX PCI Expander Bridge (PXB) */

/* Intergraph products */
#define	PCI_PRODUCT_INTERGRAPH_4D50T	0x00e4		/* Powerstorm 4D50T */

/* I. T. T. products */
#define	PCI_PRODUCT_ITT_AGX016	0x0001		/* AGX016 */
#define	PCI_PRODUCT_ITT_ITT3204	0x0002		/* ITT3204 MPEG Decoder */

#define	PCI_PRODUCT_KTI_NE2KETHER	0x3000		/* Ethernet */

/* LAN Media Corporation */
#define	PCI_PRODUCT_LMC_HSSI	0x0003		/* HSSI Interface */
#define	PCI_PRODUCT_LMC_DS3	0x0004		/* DS3 Interface */
#define	PCI_PRODUCT_LMC_SSI	0x0005		/* SSI */

/* LeadTek Research */
#define	PCI_PRODUCT_LEADTEK_S3_805	0x0000		/* S3 805 */

/* Linear Systems / CompuModules */
#define	PCI_PRODUCT_LINEARSYS_DVB_TX	0x7629		/* DVB Transmitter */
#define	PCI_PRODUCT_LINEARSYS_DVB_RX	0x7630		/* DVB Receiver */

/* Lite-On products */
#define	PCI_PRODUCT_LITEON_82C168	0x0002		/* 82C168/82C169 (PNIC) 10/100 Ethernet */
#define	PCI_PRODUCT_LITEON_82C115	0xc115		/* 82C115 (PNIC II) 10/100 Ethernet */

/* Lucent products */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0440	0x0440		/* K56flex DSVD LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0441	0x0441		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0442	0x0442		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0443	0x0443		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0444	0x0444		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0445	0x0445		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0446	0x0446		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0447	0x0447		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0448	0x0448		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0449	0x0449		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044A	0x044A		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044B	0x044B		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044C	0x044C		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044D	0x044D		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044E	0x044E		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0450	0x0450		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0451	0x0451		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0452	0x0452		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0453	0x0453		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0454	0x0454		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0455	0x0455		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0456	0x0456		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0457	0x0457		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0458	0x0458		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0459	0x0459		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_045A	0x045A		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_USBHC	0x5801		/* USB Host Controller */

/* Macronix */
#define	PCI_PRODUCT_MACRONIX_MX98713	0x0512		/* MX98713 (PMAC) 10/100 Ethernet */
#define	PCI_PRODUCT_MACRONIX_MX987x5	0x0531		/* MX987x5 (PMAC) 10/100 Ethernet */

/* Madge Networks products */
#define	PCI_PRODUCT_MADGE_COLLAGE25	0x1000		/* Collage 25 ATM adapter */
#define	PCI_PRODUCT_MADGE_COLLAGE155	0x1001		/* Collage 155 ATM adapter */

/* Matrox products */
#define	PCI_PRODUCT_MATROX_ATLAS	0x0518		/* MGA PX2085 (\"Atlas\") */
#define	PCI_PRODUCT_MATROX_MILLENNIUM	0x0519		/* MGA Millennium 2064W */
#define	PCI_PRODUCT_MATROX_MYSTIQUE	0x051a		/* MGA Mystique 1064SG */
#define	PCI_PRODUCT_MATROX_MILLENNIUM2	0x051b		/* MGA Millennium II 2164W */
#define	PCI_PRODUCT_MATROX_MILLENNIUM2_AGP	0x051f		/* MGA Millennium II 2164WA-B AG */
#define	PCI_PRODUCT_MATROX_G200_PCI	0x0520		/* MGA G200 PCI */
#define	PCI_PRODUCT_MATROX_G200_AGP	0x0521		/* MGA G200 AGP */
#define	PCI_PRODUCT_MATROX_G400_AGP	0x0525		/* MGA G400 AGP */
#define	PCI_PRODUCT_MATROX_IMPRESSION	0x0d10		/* MGA Impression */
#define	PCI_PRODUCT_MATROX_G100_PCI	0x1000		/* MGA G100 PCI */
#define	PCI_PRODUCT_MATROX_G100_AGP	0x1001		/* MGA G100 AGP */

/* Motorola products */
#define	PCI_PRODUCT_MOT_MPC105	0x0001		/* MPC105 \"Eagle\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC106	0x0002		/* MPC106 \"Grackle\" Host Bridge */

/* Mylex products */
#define	PCI_PRODUCT_MYLEX_960P	0x0001		/* DAC960P RAID controller */

/* Mutech products */
#define	PCI_PRODUCT_MUTECH_MV1000	0x0001		/* MV1000 */

#define	PCI_PRODUCT_NETVIN_5000	0x5000		/* 5000 Ethernet */

/* Newbridge / Tundra products */
#define	PCI_PRODUCT_NEWBRIDGE_CA91CX42	0x0000		/* Universe VME bridge */

/* National Semiconductor products */
#define	PCI_PRODUCT_NS_DP83810	0x0001		/* DP83810 10/100 Ethernet */
#define	PCI_PRODUCT_NS_DP83815	0x0020		/* DP83815 10/100 Ethernet */
#define	PCI_PRODUCT_NS_NS87410	0xd001		/* NS87410 */

/* NCR/Symbios Logic products */
#define	PCI_PRODUCT_SYMBIOS_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_SYMBIOS_820	0x0002		/* 53c820 */
#define	PCI_PRODUCT_SYMBIOS_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_SYMBIOS_815	0x0004		/* 53c815 */
#define	PCI_PRODUCT_SYMBIOS_810AP	0x0005		/* 53c810AP */
#define	PCI_PRODUCT_SYMBIOS_860	0x0006		/* 53c860 */
#define	PCI_PRODUCT_SYMBIOS_896	0x000b		/* 53c896 */
#define	PCI_PRODUCT_SYMBIOS_895	0x000c		/* 53c895 */
#define	PCI_PRODUCT_SYMBIOS_885	0x000d		/* 53c885 */
#define	PCI_PRODUCT_SYMBIOS_875	0x000f		/* 53c875 */
#define	PCI_PRODUCT_SYMBIOS_1510	0x0010		/* 53c1510 */
#define	PCI_PRODUCT_SYMBIOS_875J	0x008f		/* 53c875J */

/* Packet Engines products */
#define	PCI_PRODUCT_SYMBIOS_PE_GNIC	0x0702		/* Packet Engines G-NIC Ethernet */

/* NEC products */
#define	PCI_PRODUCT_NEC_USB	0x0035		/* USB Host Controller */
#define	PCI_PRODUCT_NEC_POWERVR2	0x0046		/* PowerVR PCX2 */
#define	PCI_PRODUCT_NEC_PD72872	0x0063		/* uPD72872 IEEE 1394 OHCI Host Controller */
#define	PCI_PRODUCT_NEC_PD72870	0x00cd		/* uPD72870 IEEE 1394 OHCI Host Controller */
#define	PCI_PRODUCT_NEC_PD72871	0x00ce		/* uPD72871 IEEE 1394 OHCI Host Controller */

/* Neomagic products */
#define	PCI_PRODUCT_NEOMAGIC_NMMG128ZV	0x0003		/* MagicGraph 128ZV */
#define	PCI_PRODUCT_NEOMAGIC_NMMG2160	0x0004		/* MagicGraph 128XD */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256AV_VGA	0x0005		/* MagicMedia 256AV VGA */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256ZX_VGA	0x0006		/* MagicMedia 256ZX VGA */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256AV_AU	0x8005		/* MagicMedia 256AV Audio */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256ZX_AU	0x8006		/* MagicMedia 256ZX Audio */

/* Netgear products */
#define	PCI_PRODUCT_NETGEAR_GA620	0x620a		/* GA620 Gigabit Ethernet */

/* NexGen products */
#define	PCI_PRODUCT_NEXGEN_NX82C501	0x4e78		/* NX82C501 Host-PCI Bridge */

/* NKK products */
#define	PCI_PRODUCT_NKK_NDR4600	0xA001		/* NDR4600 Host-PCI Bridge */

/* Number Nine products */
#define	PCI_PRODUCT_NUMBER9_I128	0x2309		/* Imagine-128 */
#define	PCI_PRODUCT_NUMBER9_I128_2	0x2339		/* Imagine-128 II */

/* Nvidia Corporationn products */
#define	PCI_PRODUCT_NVIDIA_RIVATNT	0x0020		/* RIVA TNT */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2	0x0028		/* RIVA TNT2 */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2U	0x0029		/* RIVA TNT2 Ultra */
#define	PCI_PRODUCT_NVIDIA_VANTA	0x002C		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2M64	0x002D		/* RIVA TNT2 Model 64 */
#define	PCI_PRODUCT_NVIDIA_ALADDINTNT2	0x00A0		/* Aladdin TNT2 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256	0x0100		/* GeForce 256 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEDDR	0x0101		/* GeForce DDR */
#define	PCI_PRODUCT_NVIDIA_QUADRO	0x0103		/* Quadro */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2	0x0150		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2DDR	0x0151		/* GeForce2 GTS (DDR) */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2BR	0x0152		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_QUADRO2	0x0153		/* Quadro2 */

/* Nvidia Corporation & SGS Thomson Microelectric */
#define	PCI_PRODUCT_NVIDIA_SGS_RIVA128	0x0018		/* Riva 128 */

/* Oak Technologies products */
#define	PCI_PRODUCT_OAKTECH_OTI1007	0x0107		/* OTI107 */

/* Olicom products */
#define	PCI_PRODUCT_OLICOM_OC2183	0x0013		/* Olicom OC-2183/2185 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2325	0x0012		/* Olicom OC-2325 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2326	0x0014		/* Olicom OC-2326 10/100-TX Ethernet */

/* Opti products */
#define	PCI_PRODUCT_OPTI_82C557	0xc557		/* 82C557 */
#define	PCI_PRODUCT_OPTI_82C558	0xc558		/* 82C558 */
#define	PCI_PRODUCT_OPTI_82C568	0xc568		/* 82C568 */
#define	PCI_PRODUCT_OPTI_82D568	0xd568		/* 82D568 */
#define	PCI_PRODUCT_OPTI_82C621	0xc621		/* 82C621 */
#define	PCI_PRODUCT_OPTI_82C822	0xc822		/* 82C822 */
#define	PCI_PRODUCT_OPTI_RM861HA	0xc861		/* RM861HA */
#define	PCI_PRODUCT_OPTI_82C700	0xc700		/* 82C700 */
#define	PCI_PRODUCT_OPTI_82C701	0xc701		/* 82C701 */

/* PC Tech products */
#define	PCI_PRODUCT_PCTECH_RZ1000	0x1000		/* RZ1000 */

/* PLX Technology products */
#define	PCI_PRODUCT_PLX_9060ES	0x906e		/* 9060ES PCI bus controller */

#define	PCI_PRODUCT_PROLAN_NE2KETHER	0x1980		/* Ethernet */

/* Promise products */
#define	PCI_PRODUCT_PROMISE_DC5030	0x5300		/* DC5030 */
#define	PCI_PRODUCT_PROMISE_ULTRA33	0x4d33		/* Ultra33/ATA Bus Master IDE Accelerator */
#define	PCI_PRODUCT_PROMISE_ULTRA66	0x4d38		/* Ultra66/ATA Bus Master IDE Accelerator */
#define	PCI_PRODUCT_PROMISE_ULTRA100	0x4d30		/* Ultra100/ATA Bus Master IDE Accelerator */

/* QLogic products */
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020		/* ISP1020 */
#define	PCI_PRODUCT_QLOGIC_ISP1022	0x1022		/* ISP1022 */
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080		/* ISP1080 */
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240		/* ISP1240 */
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100		/* ISP2100 */

/* Quantum Designs products */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8500	0x0001		/* 8500 */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8580	0x0002		/* 8580 */

/* Realtek (Creative Labs?) products */
#define	PCI_PRODUCT_REALTEK_RT8029	0x8029		/* 8029 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8129	0x8129		/* 8129 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8139	0x8139		/* 8139 10/100 Ethernet */

/* RICOH products */
#define	PCI_PRODUCT_RICOH_Rx5C465	0x0465		/* 5C465 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C466	0x0466		/* 5C466 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C475	0x0475		/* 5C475 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_RL5C476	0x0476		/* 5C476 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C477	0x0477		/* 5C477 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C478	0x0478		/* 5C478 PCI-CardBus bridge */

/* RISCom (SDL Communications, Inc?) products */
#define	PCI_PRODUCT_RISCOM_N2	0x5568		/* N2 */

/* RNS products */
#define	PCI_PRODUCT_RNS_FDDI	0x2200		/* 2200 FDDI */

/* S3 products */
#define	PCI_PRODUCT_S3_VIRGE	0x5631		/* ViRGE */
#define	PCI_PRODUCT_S3_TRIO32	0x8810		/* Trio32 */
#define	PCI_PRODUCT_S3_TRIO64	0x8811		/* Trio32/64 */
#define	PCI_PRODUCT_S3_AURORA64P	0x8812		/* Aurora64V+ */
#define	PCI_PRODUCT_S3_TRIO64UVP	0x8814		/* Trio64UV+ */
#define	PCI_PRODUCT_S3_VIRGE_VX	0x883d		/* ViRGE/VX */
#define	PCI_PRODUCT_S3_868	0x8880		/* 868 */
#define	PCI_PRODUCT_S3_928	0x88b0		/* 86C928 */
#define	PCI_PRODUCT_S3_864_0	0x88c0		/* 86C864-0 */
#define	PCI_PRODUCT_S3_864_1	0x88c1		/* 86C864-1 */
#define	PCI_PRODUCT_S3_864_2	0x88c2		/* 86C864-2 */
#define	PCI_PRODUCT_S3_864_3	0x88c3		/* 86C864-3 */
#define	PCI_PRODUCT_S3_964_0	0x88d0		/* 86C964-0 */
#define	PCI_PRODUCT_S3_964_1	0x88d1		/* 86C964-1 */
#define	PCI_PRODUCT_S3_964_2	0x88d2		/* 86C964-2 */
#define	PCI_PRODUCT_S3_964_3	0x88d3		/* 86C964-3 */
#define	PCI_PRODUCT_S3_968_0	0x88f0		/* 86C968-0 */
#define	PCI_PRODUCT_S3_968_1	0x88f1		/* 86C968-1 */
#define	PCI_PRODUCT_S3_968_2	0x88f2		/* 86C968-2 */
#define	PCI_PRODUCT_S3_968_3	0x88f3		/* 86C968-3 */
#define	PCI_PRODUCT_S3_TRIO64V2_DX	0x8901		/* Trio64V2/DX */
#define	PCI_PRODUCT_S3_PLATO_PX	0x8901		/* Plato/PX */
#define	PCI_PRODUCT_S3_TRIO3D	0x8904		/* 86C365 Trio3D */
#define	PCI_PRODUCT_S3_VIRGE_DX	0x8a01		/* ViRGE/DX */
#define	PCI_PRODUCT_S3_VIRGE_GX2	0x8a10		/* ViRGE/GX2 */
#define	PCI_PRODUCT_S3_TRIO3D2X	0x8a13		/* Trio3D/2X */
#define	PCI_PRODUCT_S3_SAVAGE3D	0x8a20		/* Savage3D */
#define	PCI_PRODUCT_S3_SAVAGE3D_MV	0x8a21		/* Savage3D+MV */
#define	PCI_PRODUCT_S3_SAVAGE4	0x8a22		/* Savage4 */
#define	PCI_PRODUCT_S3_VIRGE_MX	0x8c01		/* ViRGE/MX */
#define	PCI_PRODUCT_S3_VIRGE_MXP	0x8c03		/* ViRGE/MXP */
#define	PCI_PRODUCT_S3_SAVAGE_MX_MV	0x8c10		/* Savage/MX+MV */
#define	PCI_PRODUCT_S3_SAVAGE_MX	0x8c11		/* Savage/MX */
#define	PCI_PRODUCT_S3_SAVAGE_IX_MV	0x8c12		/* Savage/IX+MV */
#define	PCI_PRODUCT_S3_SAVAGE_IX	0x8c13		/* Savage/IX */
#define	PCI_PRODUCT_S3_SAVAGE2000	0x9102		/* Savage2000 */
#define	PCI_PRODUCT_S3_SONICVIBES	0xca00		/* SonicVibes */

/* Samsung Semiconductor products */
#define	PCI_PRODUCT_SAMSUNGSEMI_KS8920	0x8920		/* KS8920 10/100 Ethernet */

/* SGI products */
#define	PCI_PRODUCT_SGI_IOC3	0x0003		/* IOC3 */
#define	PCI_PRODUCT_SGI_RAD1	0x0005		/* PsiTech RAD1 */
#define	PCI_PRODUCT_SGI_TIGON	0x0009		/* Tigon Gigabit Ethernet */

/* SGS Thomson products */
#define	PCI_PRODUCT_SGSTHOMSON_2000	0x0008		/* STG 2000X */
#define	PCI_PRODUCT_SGSTHOMSON_1764	0x1746		/* STG 1764X */

/* SiByte, Inc. products */
#define	PCI_PRODUCT_SIBYTE_SB1250_PCI	0x0001		/* BCM1250 PCI Host Bridge */
#define	PCI_PRODUCT_SIBYTE_SB1250_LDT	0x0002		/* BCM1250 HyperTransport Host Bridge */

/* Sigma Designs products */
#define	PCI_PRODUCT_SIGMA_HOLLYWOODPLUS	0x8300		/* REALmagic Hollywood-Plus MPEG-2 Decoder */

/* Silicon Integrated System products */
#define	PCI_PRODUCT_SIS_86C201	0x0001		/* 86C201 */
#define	PCI_PRODUCT_SIS_86C202	0x0002		/* 86C202 */
#define	PCI_PRODUCT_SIS_86C205	0x0005		/* 86C205 */
#define	PCI_PRODUCT_SIS_85C503	0x0008		/* 85C503 or 5597/5598 ISA bridge */
#define	PCI_PRODUCT_SIS_600PMC	0x0009		/* 600 Power Mngmt Controller */
#define	PCI_PRODUCT_SIS_5597_VGA	0x0200		/* 5597/5598 integrated VGA */
#define	PCI_PRODUCT_SIS_85C501	0x0406		/* 85C501 */
#define	PCI_PRODUCT_SIS_85C496	0x0496		/* 85C496 */
#define	PCI_PRODUCT_SIS_530HB	0x0530		/* 530 Host to PCI Bridge */
#define	PCI_PRODUCT_SIS_85C601	0x0601		/* 85C601 */
#define	PCI_PRODUCT_SIS_900	0x0900		/* SiS 900 10/100 Ethernet */
#define	PCI_PRODUCT_SIS_5597_IDE	0x5513		/* 5597/5598 IDE controller */
#define	PCI_PRODUCT_SIS_5597_HB	0x5597		/* 5597/5598 host bridge */
#define	PCI_PRODUCT_SIS_530VGA	0x6306		/* 530 GUI Accelerator+3D */
#define	PCI_PRODUCT_SIS_6326	0x6326		/* 6326 AGP VGA */
#define	PCI_PRODUCT_SIS_5597_USB	0x7001		/* 5597/5598 USB host controller */
#define	PCI_PRODUCT_SIS_7016	0x7016		/* SiS 7016 10/100 Ethernet */

/* Silicon Motion products */
#define	PCI_PRODUCT_SILMOTION_LYNX_E	0x0810		/* Lynx E */

/* SMC products */
#define	PCI_PRODUCT_SMC_37C665	0x1000		/* FDC 37C665 */
#define	PCI_PRODUCT_SMC_37C922	0x1001		/* FDC 37C922 */
#define	PCI_PRODUCT_SMC_83C170	0x0005		/* 83C170 (\"EPIC/100\") Fast Ethernet */
#define	PCI_PRODUCT_SMC_83C175	0x0006		/* 83C175 (\"EPIC/100\") Fast Ethernet */

/* Solidum Systems Corporation */
#define	PCI_PRODUCT_SOLIDUM_AMD971	0x2000		/* SNP8023: AMD 971 */
#define	PCI_PRODUCT_SOLIDUM_CLASS802	0x8023		/* SNP8023: Classifier Engine */

/* Sony products */
#define	PCI_PRODUCT_SONY_CXD1947A	0x8009		/* CXD1947A IEEE 1394 Host Controller */
#define	PCI_PRODUCT_SONY_CXD32222	0x8039		/* CXD3222 OHCI IEEE 1394 Host Controller */
#define	PCI_PRODUCT_SONY_MEMSTICK	0x808a		/* Memory Stick I/F Controller */

/* Sun Microsystems products */
#define	PCI_PRODUCT_SUN_EBUS	0x1000		/* PCIO Ebus2 */
#define	PCI_PRODUCT_SUN_HMENETWORK	0x1001		/* PCIO Happy Meal Ethernet */
#define	PCI_PRODUCT_SUN_SIMBA	0x5000		/* Simba PCI bridge */
#define	PCI_PRODUCT_SUN_MS_IIep	0x9000		/* microSPARC IIep PCI */
#define	PCI_PRODUCT_SUN_US_IIi	0xa000		/* UltraSPARC IIi PCI */

/* Sundance Technology products */
#define	PCI_PRODUCT_SUNDANCETI_ST201	0x0201		/* ST201 10/100 Ethernet */

/* Surecom Technology products */
#define	PCI_PRODUCT_SURECOM_NE34	0x0e34		/* NE-34 Ethernet */

/* Symphony Labs products */
#define	PCI_PRODUCT_SYMPHONY_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_82C103	0x0103		/* 82C103 */
#define	PCI_PRODUCT_SYMPHONY_82C105	0x0105		/* 82C105 */
#define	PCI_PRODUCT_SYMPHONY2_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_83C553	0x0565		/* 83C553 PCI-ISA Bridge */

/* Schneider & Koch (really SysKonnect) products */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SKNET_FDDI	0x4000		/* SK-NET FDDI-xP */

/* Tekram Technology products (1st PCI Vendor ID)*/
#define	PCI_PRODUCT_TEKRAM_DC290	0xdc29		/* DC-290(M) */

/* Tekram Technology products (2nd PCI Vendor ID) */
#define	PCI_PRODUCT_TEKRAM2_DC690C	0x690c		/* DC-690C */

/* Texas Instruments products */
#define	PCI_PRODUCT_TI_TLAN	0x0500		/* TLAN */
#define	PCI_PRODUCT_TI_TVP4020	0x3d07		/* TVP4020 Permedia 2 */
#define	PCI_PRODUCT_TI_TSB12LV21	0x8000		/* TSB12LV21 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV22	0x8009		/* TSB12LV22 OHCI IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV23	0x8019		/* TSB12LV23 OHCI IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV26	0x8020		/* TSB12LV26 OHCI IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI1130	0xac12		/* PCI1130 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1031	0xac13		/* PCI1031 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_TI_PCI1131	0xac15		/* PCI1131 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1250	0xac16		/* PCI1250 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1220	0xac17		/* PCI1220 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1221	0xac19		/* PCI1221 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1450	0xac1b		/* PCI1450 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1225	0xac1c		/* PCI1225 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1251	0xac1d		/* PCI1251 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1211	0xac1e		/* PCI1211 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1251B	0xac1f		/* PCI1251B PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI2030	0xac20		/* PCI2030 PCI-PCI Bridge */
#define	PCI_PRODUCT_TI_PCI1420	0xac51		/* PCI1420 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1451	0xac52		/* PCI1451 PCI-CardBus Bridge */

/* Toshiba America products */
#define	PCI_PRODUCT_TOSHIBA_R4X00	0x0009		/* R4x00 Host-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA_TC35856F	0x0020		/* TC35856F ATM (\"Meteor\") */

/* Toshiba America Info Systems products */
#define	PCI_PRODUCT_TOSHIBA2_HOST	0x0601		/* Host Bridge/Controller */
#define	PCI_PRODUCT_TOSHIBA2_ISA	0x0602		/* ISA Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95	0x0603		/* ToPIC95 CardBus-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95B	0x060a		/* ToPIC95B CardBus-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC97	0x060f		/* ToPIC97 CardBus-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC100	0x0617		/* ToPIC100 CardBus-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA2_FIRO	0x0701		/* Fast Infrared Type O */

/* Trident products */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADE_I7	0x8420		/* CyberBlade i7 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9320	0x9320		/* TGUI 9320 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9350	0x9350		/* TGUI 9350 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9360	0x9360		/* TGUI 9360 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397	0x9397		/* CYBER 9397 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397DVD	0x939a		/* CYBER 9397DVD */
#define	PCI_PRODUCT_TRIDENT_CYBER_9525	0x9525		/* CYBER 9525 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9420	0x9420		/* TGUI 9420 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9440	0x9440		/* TGUI 9440 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9660	0x9660		/* TGUI 9660 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9680	0x9680		/* TGUI 9680 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9682	0x9682		/* TGUI 9682 */

/* Triones Technologies products */
/* The 366 and 370 controllers have the same product ID */
#define	PCI_PRODUCT_TRIONES_HPT366	0x0004		/* HPT366/370 IDE Controller */

/* TriTech Microelectronics products*/
#define	PCI_PRODUCT_TRITECH_TR25202	0xfc02		/* Pyramid3D TR25202 */

/* Tseng Labs products */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_A	0x3202		/* ET4000w32p rev A */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_B	0x3205		/* ET4000w32p rev B */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_C	0x3206		/* ET4000w32p rev C */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_D	0x3207		/* ET4000w32p rev D */
#define	PCI_PRODUCT_TSENG_ET6000	0x3208		/* ET6000 */

/* UMC products */
#define	PCI_PRODUCT_UMC_UM82C881	0x0001		/* UM82C881 486 Chipset */
#define	PCI_PRODUCT_UMC_UM82C886	0x0002		/* UM82C886 ISA Bridge */
#define	PCI_PRODUCT_UMC_UM8673F	0x0101		/* UM8673F EIDE Controller */
#define	PCI_PRODUCT_UMC_UM8881	0x0881		/* UM8881 HB4 486 PCI Chipset */
#define	PCI_PRODUCT_UMC_UM82C891	0x0891		/* UM82C891 */
#define	PCI_PRODUCT_UMC_UM886A	0x1001		/* UM886A */
#define	PCI_PRODUCT_UMC_UM8886BF	0x673a		/* UM8886BF */
#define	PCI_PRODUCT_UMC_UM8710	0x8710		/* UM8710 */
#define	PCI_PRODUCT_UMC_UM8886	0x886a		/* UM8886 */
#define	PCI_PRODUCT_UMC_UM8881F	0x8881		/* UM8881F PCI-Host bridge */
#define	PCI_PRODUCT_UMC_UM8886F	0x8886		/* UM8886F PCI-ISA bridge */
#define	PCI_PRODUCT_UMC_UM8886A	0x888a		/* UM8886A */
#define	PCI_PRODUCT_UMC_UM8891A	0x8891		/* UM8891A */
#define	PCI_PRODUCT_UMC_UM9017F	0x9017		/* UM9017F */
#define	PCI_PRODUCT_UMC_UM8886N	0xe88a		/* UM8886N */
#define	PCI_PRODUCT_UMC_UM8891N	0xe891		/* UM8891N */

/* ULSI Systems products */
#define	PCI_PRODUCT_ULSI_US201	0x0201		/* US201 */

/* US Robotics products */
#define	PCI_PRODUCT_USR_3CP5609	0x1008		/* 3CP5609 PCI 16550 Modem */

/* V3 Semiconductor products */
#define	PCI_PRODUCT_V3_V292PBC	0x0292		/* V292PBC AMD290x0 Host-PCI Bridge */
#define	PCI_PRODUCT_V3_V960PBC	0x0960		/* V960PBC i960 Host-PCI Bridge */
#define	PCI_PRODUCT_V3_V96DPC	0xC960		/* V96DPC i960 (Dual) Host-PCI Bridge */

/* VIA Technologies products, from http://www.via.com.tw/ */
#define	PCI_PRODUCT_VIATECH_VT8371_HB	0x0391		/* VT8371 (Apollo KX133) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8501_MVP4	0x0501		/* VT8501 MVP4 System Controller */
#define	PCI_PRODUCT_VIATECH_VT82C505	0x0505		/* VT82C505 (Pluto) */
#define	PCI_PRODUCT_VIATECH_VT82C561	0x0561		/* VT82C561 */
#define	PCI_PRODUCT_VIATECH_VT82C586A_IDE	0x0571		/* VT82C586A IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C576	0x0576		/* VT82C576 3V */
#define	PCI_PRODUCT_VIATECH_VT82C580VP	0x0585		/* VT82C580 (Apollo VP) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C586_ISA	0x0586		/* VT82C586 (Apollo VP) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C595	0x0595		/* VT82C595 (Apollo VP2) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C596A	0x0596		/* VT82C596A (Apollo Pro) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C597	0x0597		/* VT82C597 (Apollo VP3) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C598PCI	0x0598		/* VT82C598 (Apollo MVP3) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT82C686A_ISA	0x0686		/* VT82C686A (Apollo KX133) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C691	0x0691		/* VT82C691 (Apollo Pro) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT82C693	0x0693		/* VT82C693 (Apollo Pro Plus) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT86C926	0x0926		/* VT86C926 Amazon PCI-Ethernet Controller */
#define	PCI_PRODUCT_VIATECH_VT82C570M	0x1000		/* VT82C570M (Apollo) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C570MV	0x1006		/* VT82C570M (Apollo) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C586_IDE	0x1571		/* VT82C586 (Apollo VP) IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C595_2	0x1595		/* VT82C595 (Apollo VP2) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT83C572	0x3038		/* VT83C572 USB Controller */
#define	PCI_PRODUCT_VIATECH_VT82C586_PWR	0x3040		/* VT82C586 (Apollo VP) Power Management Controller */
#define	PCI_PRODUCT_VIATECH_VT3043	0x3043		/* VT3043 (Rhine) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT82C686A_SMB	0x3057		/* VT82C686A SMBus Controller */
#define	PCI_PRODUCT_VIATECH_VT82C686A_AC97	0x3058		/* VT82C686A AC-97 Audio Controller */
#define	PCI_PRODUCT_VIATECH_VT82C686A_MC97	0x3068		/* VT82C686A MC-97 Modem Controller */
#define	PCI_PRODUCT_VIATECH_VT86C100A	0x6100		/* VT86C100A (Rhine-II) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT8371_PPB	0x8391		/* VT8371 (Apollo KX133) PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8501AGP	0x8501		/* VT8501 PCI-AGP */
#define	PCI_PRODUCT_VIATECH_VT82C597AGP	0x8597		/* VT82C597 (Apollo VP3) PCI-AGP */
#define	PCI_PRODUCT_VIATECH_VT82C598AGP	0x8598		/* VT82C598 (Apollo MVP3) PCI-AGP */

/* Vortex Computer Systems products */
/* GDT_PCI */
#define	PCI_PRODUCT_VORTEX_GDT_60x0	0x0000		/* GDT6000/6020/6050 */
#define	PCI_PRODUCT_VORTEX_GDT_6000B	0x0001		/* GDT6000B/6010 */
/* GDT_PCINEW */
#define	PCI_PRODUCT_VORTEX_GDT_6x10	0x0002		/* GDT6110/6510 */
#define	PCI_PRODUCT_VORTEX_GDT_6x20	0x0003		/* GDT6120/6520 */
#define	PCI_PRODUCT_VORTEX_GDT_6530	0x0004		/* GDT6530 */
#define	PCI_PRODUCT_VORTEX_GDT_6550	0x0005		/* GDT6550 */
/* GDT_PCINEW, wide/ultra SCSI controllers */
#define	PCI_PRODUCT_VORTEX_GDT_6x17	0x0006		/* GDT6117/6517 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27	0x0007		/* GDT6127/6527 */
#define	PCI_PRODUCT_VORTEX_GDT_6537	0x0008		/* GDT6537 */
#define	PCI_PRODUCT_VORTEX_GDT_6557	0x0009		/* GDT6557/6557-ECC */
/* GDT_PCINEW, wide SCSI controllers */
#define	PCI_PRODUCT_VORTEX_GDT_6x15	0x0010		/* GDT6115/6515 */
#define	PCI_PRODUCT_VORTEX_GDT_6x25	0x0011		/* GDT6125/6525 */
#define	PCI_PRODUCT_VORTEX_GDT_6535	0x0012		/* GDT6535 */
#define	PCI_PRODUCT_VORTEX_GDT_6555	0x0013		/* GDT6555/6555-ECC */
/* GDT_MPR, RP series, wide/ultra SCSI */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP	0x0100		/* GDT6117RP/GDT6517RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP	0x0101		/* GDT6127RP/GDT6527RP */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP	0x0102		/* GDT6537RP */
#define	PCI_PRODUCT_VORTEX_GDT_6557RP	0x0103		/* GDT6557RP */
/* GDT_MPR, RP series, narrow/ultra SCSI */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP	0x0104		/* GDT6111RP/GDT6511RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP	0x0105		/* GDT6121RP/GDT6521RP */
/* GDT_MPR, RD series, wide/ultra SCSI */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RD	0x0110		/* GDT6117RD/GDT6517RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RD	0x0111		/* GDT6127RD/GDT6527RD */
#define	PCI_PRODUCT_VORTEX_GDT_6537RD	0x0112		/* GDT6537RD */
#define	PCI_PRODUCT_VORTEX_GDT_6557RD	0x0113		/* GDT6557RD */
/* GDT_MPR, RD series, narrow/ultra SCSI */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RD	0x0114		/* GDT6111RD/GDT6511RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RD	0x0115		/* GDT6121RD/GDT6521RD */
/* GDT_MPR, RD series, wide/ultra2 SCSI */
#define	PCI_PRODUCT_VORTEX_GDT_6x18RD	0x0118		/* GDT6118RD/GDT6518RD/GDT6618RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RD	0x0119		/* GDT6128RD/GDT6528RD/GDT6628RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RD	0x011A		/* GDT6538RD/GDT6638RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RD	0x011B		/* GDT6558RD/GDT6658RD */
/* GDT_MPR, RN series (64-bit PCI), wide/ultra2 SCSI */
#define	PCI_PRODUCT_VORTEX_GDT_7x18RN	0x0168		/* GDT7118RN/GDT7518RN/GDT7618RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x28RN	0x0169		/* GDT7128RN/GDT7528RN/GDT7628RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x38RN	0x016A		/* GDT7538RN/GDT7638RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x58RN	0x016B		/* GDT7558RN/GDT7658RN */
/* GDT_MPR, RD series, Fibre Channel */
#define	PCI_PRODUCT_VORTEX_GDT_6x19RD	0x0210		/* GDT6519RD/GDT6619RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x29RD	0x0211		/* GDT6529RD/GDT6629RD */
/* GDT_MPR, RN series (64-bit PCI), Fibre Channel */
#define	PCI_PRODUCT_VORTEX_GDT_7x19RN	0x0260		/* GDT7519RN/GDT7619RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x29RN	0x0261		/* GDT7529RN/GDT7629RN */

/* VLSI products */
#define	PCI_PRODUCT_VLSI_82C592	0x0005		/* 82C592 CPU Bridge */
#define	PCI_PRODUCT_VLSI_82C593	0x0006		/* 82C593 ISA Bridge */
#define	PCI_PRODUCT_VLSI_82C594	0x0007		/* 82C594 Wildcat System Controller */
#define	PCI_PRODUCT_VLSI_82C596597	0x0008		/* 82C596/597 Wildcat ISA Bridge */
#define	PCI_PRODUCT_VLSI_82C541	0x000c		/* 82C541 */
#define	PCI_PRODUCT_VLSI_82C543	0x000d		/* 82C543 */
#define	PCI_PRODUCT_VLSI_82C532	0x0101		/* 82C532 */
#define	PCI_PRODUCT_VLSI_82C534	0x0102		/* 82C534 */
#define	PCI_PRODUCT_VLSI_82C535	0x0104		/* 82C535 */
#define	PCI_PRODUCT_VLSI_82C147	0x0105		/* 82C147 */
#define	PCI_PRODUCT_VLSI_82C975	0x0200		/* 82C975 */
#define	PCI_PRODUCT_VLSI_82C925	0x0280		/* 82C925 */

/* Weitek products */
#define	PCI_PRODUCT_WEITEK_P9000	0x9001		/* P9000 */
#define	PCI_PRODUCT_WEITEK_P9100	0x9100		/* P9100 */

/* Western Digital products */
#define	PCI_PRODUCT_WD_WD33C193A	0x0193		/* WD33C193A */
#define	PCI_PRODUCT_WD_WD33C196A	0x0196		/* WD33C196A */
#define	PCI_PRODUCT_WD_WD33C197A	0x0197		/* WD33C197A */
#define	PCI_PRODUCT_WD_WD7193	0x3193		/* WD7193 */
#define	PCI_PRODUCT_WD_WD7197	0x3197		/* WD7197 */
#define	PCI_PRODUCT_WD_WD33C296A	0x3296		/* WD33C296A */
#define	PCI_PRODUCT_WD_WD34C296	0x4296		/* WD34C296 */
#define	PCI_PRODUCT_WD_90C	0xC24A		/* 90C */

/* Winbond Electronics products */
#define	PCI_PRODUCT_WINBOND_W83769F	0x0001		/* W83769F */
#define	PCI_PRODUCT_WINBOND_W89C840F	0x0840		/* W89C840F 10/100 Ethernet */
#define	PCI_PRODUCT_WINBOND_W89C940F	0x0940		/* W89C940F Ethernet */
#define	PCI_PRODUCT_WINBOND_W89C940F_1	0x5a5a		/* W89C940F Ethernet */

/* Xircom products */
/* is the `-3' here just indicating revision 3, or is it really part
   of the device name? */
#define	PCI_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 Fast Ethernet Controller */
/* this is the device id `indicating 21143 driver compatibility' */
#define	PCI_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 Fast Ethernet Controller (21143) */

/* Yamaha products */
#define	PCI_PRODUCT_YAMAHA_YMF724	0x0004		/* 724 Audio */
#define	PCI_PRODUCT_YAMAHA_YMF740	0x000A		/* 740 Audio */
#define	PCI_PRODUCT_YAMAHA_YMF740C	0x000C		/* 740C (DS-1) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF724F	0x000D		/* 724F (DS-1) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF744B	0x0010		/* 744 (DS-1S) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF754	0x0012		/* 754 (DS-1E) Audio */

/* Zeinet products */
#define	PCI_PRODUCT_ZEINET_1221	0x0001		/* 1221 */

/* Ziatech products */
#define	PCI_PRODUCT_ZIATECH_ZT8905	0x8905		/* PCI-ST32 Bridge */

/* Zoran products */
#define	PCI_PRODUCT_ZORAN_ZR36120	0x6120		/* Video Controller */
