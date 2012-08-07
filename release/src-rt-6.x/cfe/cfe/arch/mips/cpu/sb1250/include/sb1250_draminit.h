/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  DRAM Initialization routine definitions	File: sb1250_draminit.h
    *  
    *  This file contains constants and data structures specific to
    *  the operation of sb1250_draminit.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
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

#ifndef _SB1250_DEFS_H
#include "sb1250_defs.h"		/* needed for _SB_MAKExx macros */
#endif

#ifndef _SB1250_MC_H
#include "sb1250_mc.h"			/* memory controller constants */
#endif


/*  *********************************************************************
    *  DRAMINFO macros - used to construct the DRAM information
    *  table.  There are two flavors, for "C" and assembly.
    ********************************************************************* */


/*
 * This macro generates a "decimals and tenths" hex value like those used
 * in an SPD.  'x' is the decimal portion and 't' is the tenths.  It's
 * sort of like a BCD value except 'x' can contain 0..15
 *
 * This macro needs to be used for the tCK parameter.
 *
 * For example, to specify 7.5, you could say DRT10(7,5)
 */

#define DRT10(x,t) (((x)<<4)|(t))

/*
 * This macro generates "decimals and quarters" like those used
 * in an SPD.  The 'q' parameter should be 0, 25, 50, or 75
 * This macro needs to be used by tRP, tRRD, rRCD, tRFC, adn tRC
 * 
 * For example, to specify 20.25, you could say DRT4(20,25)
 */

#define DRT4(x,q) (((x)<<2)|((q)/25))



#ifdef __ASSEMBLER__
#define DRAM_GLOBALS(chintlv) \
          .byte MCR_GLOBALS,chintlv,0,0,0,0,0,0,0,0,0,0 ;
#define DRAM_CHAN_CFG(chan,tMEMCLK,dramtype,pagepolicy,blksize,csintlv,ecc,flg) \
          .byte MCR_CHCFG,chan,tMEMCLK,dramtype,pagepolicy,blksize,csintlv,ecc,flg,0,0,0 ;
#define DRAM_CHAN_CFG2(chan,tMEMCLK,tROUNDTRIP,dramtype,pagepolicy,blksize,csintlv,ecc,flg) \
          .byte MCR_CHCFG,chan,tMEMCLK,dramtype,pagepolicy,blksize,csintlv,ecc,flg,tROUNDTRIP,0,0 ;
#define DRAM_CHAN_CLKCFG(addrskew,dqoskew,dqiskew,addrdrive,datadrive,clkdrive) \
          .byte MCR_CLKCFG,addrskew,dqoskew,dqiskew,addrdrive,datadrive,clkdrive,0,0,0,0,0 ;
#define DRAM_CHAN_MANTIMING(tCK,rfsh,tval) \
          .byte MCR_MANTIMING,tCK,rfsh,0 ; \
          .byte (((tval) >> 56)&0xFF), (((tval) >> 48) & 0xFF) ; \
          .byte (((tval) >> 40)&0xFF), (((tval) >> 32) & 0xFF) ; \
          .byte (((tval) >> 24)&0xFF), (((tval) >> 16) & 0xFF) ; \
          .byte (((tval) >>  8)&0xFF), (((tval) >>  0) & 0xFF) ; 
#define DRAM_CS_TIMING(tCK,rfsh,caslatency,attributes,tRAS,tRP,tRRD,tRCD,tRFC,tRC) \
          .byte MCR_TIMING,tCK,rfsh,caslatency,attributes,tRAS,tRP,tRRD,tRCD,tRFC,tRC,0 ;
#define DRAM_CS_GEOM(csel,rows,cols,banks) \
          .byte MCR_GEOM,csel,rows,cols,banks,0,0,0,0,0,0,0 ;
#define DRAM_CS_SPD(csel,flags,chan,dev) \
          .byte MCR_SPD,csel,flags,chan,dev,0,0,0,0,0,0,0 ;
#define DRAM_EOT \
          .byte MCR_EOT,0,0,0,0,0,0,0,0,0,0,0 ;
#else
#define DRAM_GLOBALS(chintlv) \
          {MCR_GLOBALS,chintlv,0,0,0,0,0,0,0,0,0,0}
#define DRAM_CHAN_CFG(chan,tMEMCLK,dramtype,pagepolicy,blksize,csintlv,ecc,flg) \
          {MCR_CHCFG,chan,tMEMCLK,dramtype,pagepolicy,blksize,csintlv,ecc,flg,0,0,0}
#define DRAM_CHAN_CFG2(chan,tMEMCLK,tROUNDTRIP,dramtype,pagepolicy,blksize,csintlv,ecc,flg) \
          {MCR_CHCFG,chan,tMEMCLK,dramtype,pagepolicy,blksize,csintlv,ecc,flg,tROUNDTRIP,0,0}
#define DRAM_CHAN_CLKCFG(addrskew,dqoskew,dqiskew,addrdrive,datadrive,clkdrive) \
          {MCR_CLKCFG,addrskew,dqoskew,dqiskew,addrdrive,datadrive,clkdrive,0,0,0,0,0}
#define DRAM_CHAN_MANTIMING(tCK,rfsh,tval) \
          {MCR_MANTIMING,tCK,rfsh,0,  \
          (((tval) >> 56)&0xFF), (((tval) >> 48) & 0xFF),  \
          (((tval) >> 40)&0xFF), (((tval) >> 32) & 0xFF),  \
          (((tval) >> 24)&0xFF), (((tval) >> 16) & 0xFF),  \
          (((tval) >>  8)&0xFF), (((tval) >>  0) & 0xFF) } 
#define DRAM_CS_TIMING(tCK,rfsh,caslatency,attributes,tRAS,tRP,tRRD,tRCD,tRFC,tRC) \
          {MCR_TIMING,tCK,rfsh,caslatency,attributes,tRAS,tRP,tRRD,tRCD,tRFC,tRC,0}
#define DRAM_CS_GEOM(csel,rows,cols,banks) \
          {MCR_GEOM,csel,rows,cols,banks,0,0,0,0,0,0,0}
#define DRAM_CS_SPD(csel,flags,chan,dev) \
          {MCR_SPD,csel,flags,chan,dev,0,0,0,0,0,0,0}
#define DRAM_EOT \
          {MCR_EOT,0,0,0,0,0,0,0,0,0,0,0}
#endif


#define MCR_GLOBALS     0
#define MCR_CHCFG 	1
#define MCR_TIMING	2
#define MCR_CLKCFG	3
#define MCR_GEOM	4
#define MCR_SPD		5
#define MCR_MANTIMING	6
#define MCR_EOT		0xFF

#ifndef __ASSEMBLER__
typedef struct mc_initrec_s {
    uint8_t mcr_type;			/* record type */
    uint8_t mcr_reserved1;		/* pad to 12 bytes */
    uint8_t mcr_reserved2;		/* pad to 12 bytes */
    uint8_t mcr_reserved3;		/* pad to 12 bytes */
    uint8_t mcr_reserved4;		/* pad to 12 bytes */
    uint8_t mcr_reserved5;		/* pad to 12 bytes */
    uint8_t mcr_reserved6;		/* pad to 12 bytes */
    uint8_t mcr_reserved7;		/* pad to 12 bytes */
    uint8_t mcr_reserved8;		/* pad to 12 bytes */
    uint8_t mcr_reserved9;		/* pad to 12 bytes */
    uint8_t mcr_reserved10;		/* pad to 12 bytes */
    uint8_t mcr_reserved11;		/* pad to 12 bytes */
} mc_initrec_t;

typedef struct mc_globals_s {
    uint8_t gbl_type;
    uint8_t gbl_intlv_ch;		/* true to interleave channels */
    uint8_t gbl_reserved[10];		/* pad to 12 bytes */
} mc_globals_t;

typedef struct mc_mantiming_s {
    uint8_t mtm_type;
    uint8_t mtm_tCK;
    uint8_t mtm_rfsh;
    uint8_t mtm_reserved;
    uint8_t mtm_timing[8];		/* one 64-bit word, as bytes */
} mc_mantiming_t;

typedef struct mc_cfgdata_s {
    uint8_t cfg_type;
    uint8_t cfg_chan;
    uint8_t cfg_mintmemclk;
    uint8_t cfg_dramtype;
    uint8_t cfg_pagepolicy;
    uint8_t cfg_blksize;
    uint8_t cfg_intlv_cs;
    uint8_t cfg_ecc;
    uint8_t cfg_flags;
    uint8_t cfg_roundtrip;
    uint8_t cfg_reserved[2];
} mc_cfgdata_t;

typedef struct mc_clkcfg_s {
    uint8_t clk_type;
    uint8_t clk_addrskew;
    uint8_t clk_dqoskew;
    uint8_t clk_dqiskew;
    uint8_t clk_addrdrive;
    uint8_t clk_datadrive;
    uint8_t clk_clkdrive;
    uint8_t clk_reserved[5];
} mc_clkcfg_t;

typedef struct mc_timingdata_s {
    uint8_t tmg_type;			/* record type */
    uint8_t tmg_tCK;			/* tCK we should use */
    uint8_t tmg_rfsh;			/* SPD[12] Refresh Rate */
    uint8_t tmg_caslatency;		/* SPD[18] CAS Latencies Supported (set only one bit) */
    uint8_t tmg_attributes;		/* SPD[21] Attributes */
    uint8_t tmg_tRAS;			/* SPD[30] */
    uint8_t tmg_tRP;			/* SPD[27] */
    uint8_t tmg_tRRD;			/* SPD[28] */
    uint8_t tmg_tRCD;			/* SPD[29] */
    uint8_t tmg_tRFC;			/* SPD[42] */
    uint8_t tmg_tRC;			/* SPD[41] */
    uint8_t tmg_reserved;		/* not used */
} mc_timingdata_t;

typedef struct mc_geomdata_s {
    uint8_t geom_type;			/* record type */
    uint8_t geom_csel;			/* Chip Select number */
    uint8_t geom_rows;			/* rows */
    uint8_t geom_cols;			/* columns */
    uint8_t geom_banks;			/* banks */
    uint8_t geom_reserved[7];		/* pad to 12 bytes */
} mc_geomdata_t;

typedef struct mc_spddata_s {
    uint8_t spd_type;			/* record type */
    uint8_t spd_csel;			/* Chip Select number */
    uint8_t spd_flags;			/* flags */
    uint8_t spd_smbuschan;		/* SMBus Channel */
    uint8_t spd_smbusdev;		/* SMBus device */
    uint8_t spd_reserved[7];		/* pad to 12 bytes */
} mc_spddata_t;

typedef union draminittab_s {
    mc_initrec_t mcr;			/* record with just header */
    mc_globals_t gbl;			/* global data */
    mc_cfgdata_t cfg;			/* channel configuration data */
    mc_clkcfg_t clk;			/* clock configuration */
    mc_timingdata_t tmg;		/* timing data */
    mc_geomdata_t geom;			/* geometry data */
    mc_spddata_t spd;			/* SPD data */
    mc_mantiming_t mtm;			/* manual timing */
    uint8_t raw[12];			/* RAW data */
} draminittab_t;

#endif



/*  *********************************************************************
    *  JEDEC constants (serial presence detect offsets and bit fields)
    ********************************************************************* */

#define JEDEC_SPD_MEMTYPE	2	/* memory type (sdram, ddr, etc.) */
#define JEDEC_SPD_ROWS		3	/* row bits */
#define JEDEC_SPD_COLS		4	/* column bits */
#define JEDEC_SPD_SIDES		5	/* number of sides */
#define JEDEC_SPD_WIDTH		6	/* and 7, bit width of device */	
#define JEDEC_SPD_BANKS		17	/* number of banks */
#define JEDEC_SPD_DENSITY	31	/* module bank density */

#define JEDEC_SPD_tCK25		9	/* tCK @ CAS 2.5 */
#define JEDEC_SPD_tCK20		23	/* tCK @ CAS 2.0 */
#define JEDEC_SPD_tCK10		25	/* tCK @ CAS 1.0 */
#define JEDEC_SPD_RFSH		12	/* refresh rate */
#define JEDEC_SPD_CASLATENCIES	18	/* CAS Latencies supported */
#define JEDEC_SPD_ATTRIBUTES	21	/* module attributes */
#define JEDEC_SPD_tRAS		30	/* tRAS */
#define JEDEC_SPD_tRP		27	/* tRP */
#define JEDEC_SPD_tRRD		28	/* tRRD */
#define JEDEC_SPD_tRCD		29	/* tRCD */
#define JEDEC_SPD_tRFC		42	/* tRFC */
#define JEDEC_SPD_tRC		41	/* tRC */

#define JEDEC_SPD_MAX		44	/* highest SPD value we will read */

#define JEDEC_MEMTYPE_DDRSDRAM	1
#define JEDEC_MEMTYPE_DDRSDRAM2	7
#define JEDEC_MEMTYPE_SDRAM	4
#define SPD_MEMTYPE_FCRAM	8	/* CFE specific to support FCRAM dimms */

#define JEDEC_CASLAT_35		0x20
#define JEDEC_CASLAT_30		0x10
#define JEDEC_CASLAT_25		0x08
#define JEDEC_CASLAT_20		0x04
#define JEDEC_CASLAT_15		0x02
#define JEDEC_CASLAT_10		0x01

#define JEDEC_ATTRIB_REG	0x02

#define JEDEC_RFSH_MASK		0x7F
#define JEDEC_RFSH_64khz	0
#define JEDEC_RFSH_256khz	1
#define JEDEC_RFSH_128khz	2
#define JEDEC_RFSH_32khz	3
#define JEDEC_RFSH_16khz	4
#define JEDEC_RFSH_8khz		5

/*  *********************************************************************
    *  Configuration parameter values and flag shorthands to improve
    *  readability of the tables.
    ********************************************************************* */


#define CLOSED			K_MC_CS_ATTR_CLOSED
#define CASCHECK		K_MC_CS_ATTR_CASCHECK
#define HINT			K_MC_CS_ATTR_HINT
#define OPEN			K_MC_CS_ATTR_OPEN

#define JEDEC			K_MC_DRAM_TYPE_JEDEC
#define FCRAM			K_MC_DRAM_TYPE_FCRAM
#define SGRAM			K_MC_DRAM_TYPE_SGRAM
#define DRAM_TYPE_SPD	3

#define ECCENABLE	1	/* ECC enable DRAM_CHANCFG */
#define ECCDISABLE	0

#define BLKSIZE32	32	/* Block size */
#define BLKSIZE64	64
#define BLKSIZE128	128

#define MCFLG_ECC_ENABLE 1     /* flags for channel configuration */
#define MCFLG_BIGMEM	2
#define MCFLG_FORCEREG	4

#define MC_CHAN0	0      /* shorthands for readability */
#define MC_CHAN1	1

#define MC_NOPORTINTLV	0      /* Used in DRAM_GLOBALS() */
#define MC_PORTINTLV	1

#define MC_CS0		0      /* shorthands for readability */
#define MC_CS1		1
#define MC_CS2		2
#define MC_CS3		3

#define NOCSINTLV	0	/* Chip select interleave values */
#define CSINTLV1	1
#define CSINTLV2	2

/*
 * End
 */
