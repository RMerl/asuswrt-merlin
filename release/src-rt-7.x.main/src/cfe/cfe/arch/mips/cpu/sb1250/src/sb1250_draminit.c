/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  DRAM Startup Module  		      File: sb1250_draminit.c
    *  
    *  This module contains code to initialize and start the DRAM
    *  controller on the SB1250.
    * 
    *  This is the fancy new init module, written in "C".
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

/*
 * This code can be linked into non-CFE, non-SB1250 things like SOCVIEW, a JTAG
 * tool.  In that case it's not even running on a 1250, but we can
 * borrow the code to generate timing values for us.
 * 
 * The _MCSTANDALONE_ ifdef is normally turned *off* for firmware use,
 * but programs like "memconfig" (CFE host tool) or SOCVIEW use it
 * to allow us to run the memory initialization outside a 1250.
 */

#ifdef _MCSTANDALONE_
#include <stdio.h>
#else
#include "sbmips.h"
#endif

#include "sb1250_regs.h"
#include "sb1250_mc.h"
#include "sb1250_smbus.h"
#include "sb1250_scd.h"

/*
 * Uncomment to use data mover to zero memory 
 * Note: this is not a good idea in Pass1, since we'll
 * be running cacheable noncoherent at this point in the
 * CFE init sequence.
 */
/* #define _DMZERO_ */

#ifdef _DMZERO_
#include "sb1250_dma.h"
#endif

/*  *********************************************************************
    *  Magic Constants
    ********************************************************************* */

/* 
 * This constant represents the "round trip" time of your board.
 * Measured from the pins on the BCM1250, it is the time from the
 * rising edge of the MCLK pin to the rising edge of the DQS coming
 * back from the memory.
 *
 * It is used in the calculation of which cycle responses are expected
 * from the memory for a given request.  The units are in tenths of
 * nanoseconds.
 */

#define DEFAULT_MEMORY_ROUNDTRIP_TIME	25		/* 2.5ns (default) */
#define DEFAULT_MEMORY_ROUNDTRIP_TIME_FCRAM	20	/* 2.0ns for FCRAM */


#define PASS1_DLL_SCALE_NUMERATOR	30		/* 30/400 = 0.075 */
#define PASS1_DLL_SCALE_DENOMINATOR	400
#define PASS1_DLL_OFFSET		63		/* 63/400 = 0.1575 */

#define PASS2_DLL_SCALE_NUMERATOR	30		/* 30/400 = 0.075 */
#define PASS2_DLL_SCALE_DENOMINATOR	400
#define PASS2_DLL_OFFSET		63		/* 63/400 = 0.1575 */


/*
 * The constants below were created by careful measurement of 
 * BCM1250 parts.  The units are in tenths of nanoseconds
 * to be compatible with the rest of the calculations in sb1250_auto_timing.
 */

#define SB1250_MIN_R2W_TIME		30	/* 3.0 ns */
#define SB1250_MIN_DQS_MARGIN		25
#define SB1250_WINDOW_OPEN_OFFSET	18
#define SB1250_CLOSE_01_OFFSET		34
#define SB1250_CLOSE_02_OFFSET		22
#define SB1250_CLOSE_12_OFFSET		24



/*  *********************************************************************
    *  Basic types
    ********************************************************************* */

#ifdef _CFE_
#include "lib_types.h"
#else
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

/*
 * For SOCVIEW and non-CFE, non-MIPS stuff, make sure the "port"
 * data type is 64 bits.  Otherwise we take our cue from 'long'
 * which will be pointer-sized.
 */

#if defined(_MCSTANDALONE_)
typedef long long sbport_t;
#else
typedef long sbport_t;
#endif

#ifdef _CFE_
#include "bsp_config.h"
#endif

#define TRUE 1
#define FALSE 0

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*
 * This module needs to be compiled with mips64 to ensure that 64-bit
 * values are in 64-bit registers and that reads/writes of 64-bit numbers
 * are done with the ld/sd instructions.  
 */
#if !defined(__mips64) && !defined(_MCSTANDALONE_)
#error "This module MUST be compiled with __mips64.  See the comments for details."
#endif

/*
 * Configure some stuff here if not running under the firmware.
 */

#ifndef _CFE_
#define CFG_DRAM_ECC		0
#define CFG_DRAM_SMBUS_CHANNEL	0
#define CFG_DRAM_SMBUS_BASE	0x54
#define CFG_DRAM_BLOCK_SIZE	32
#endif


/*
 * Clock configuration parameters, except for the MCLK ratio
 * which is set according to the value of the PLL divide ratio.
 */

#define V_MC_CLKCONFIG_VALUE_PASS1   V_MC_ADDR_SKEW(0x0F) | \
                                     V_MC_DQO_SKEW(0x8) | \
                                     V_MC_DQI_SKEW(0x8) | \
                                     V_MC_ADDR_DRIVE(0xF) | \
                                     V_MC_DATA_DRIVE(0xF) | \
                                     V_MC_CLOCK_DRIVE(0) 

#define V_MC_CLKCONFIG_VALUE         V_MC_ADDR_SKEW(0x08) | \
                                     V_MC_DQO_SKEW(0x8) | \
                                     V_MC_DQI_SKEW(0x8) | \
                                     V_MC_ADDR_DRIVE(0xF) | \
                                     V_MC_DATA_DRIVE(0xF) | \
                                     V_MC_CLOCK_DRIVE(0xF) 

/*
 * These belong in some SB1250-specific file I'm sure.
 */

#define MC_CHANNELS	2		/* we have two channels */
#define MC_CHIPSELS	4		/* and four chipsels per channel */


/*  *********************************************************************
    *  Reference Clock
    ********************************************************************* */

#ifdef _MAGICWID_
  /* 
   * You really don't want to know about this.  During testing, we futz
   * with the 100mhz clock and store the actual speed of the clock
   * in the PromICE so we can make the calculations work out correctly
   * (and automatically)
   */
  #define SB1250_REFCLK (*((uint64_t *) PHYS_TO_K1(0x1FC00018)))
  #undef K_SMB_FREQ_100KHZ
  #define K_SMB_FREQ_100KHZ ((SB1250_REFCLK*10)/8)
#else
  /*
   * If non-CFE, non-MIPS, make the refclk an input parameter.
   */
  #if defined(_MCSTANDALONE_)
    int sb1250_refclk = 100;
    int dram_cas_latency;
    int dram_tMemClk;
    #define SB1250_REFCLK sb1250_refclk
  #endif
#endif

/*
 * Define our reference clock.  The default is 100MHz unless
 * overridden.  You can override this in your bsp_config.h file.
 */

#ifdef SB1250_REFCLK_HZ
   #define SB1250_REFCLK ((SB1250_REFCLK_HZ)/1000000)
#endif

#ifndef SB1250_REFCLK
  #define SB1250_REFCLK	100		/* speed of refclk, in Mhz */
#endif

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

/*
 * For the general case, reads/writes to MC CSRs are just pointer
 * references.  In SOCVIEW and other non-CFE, non-MIPS programs, we hook the
 * read/write calls to let us supply the data from somewhere else.
 */

#if defined(_MCSTANDALONE_)
  #define PHYS_TO_K1(x) (x)
  #define WRITECSR(csr,val) sbwritecsr(csr,val)
  #define READCSR(csr)      sbreadcsr(csr)
  extern void sbwritecsr(uint64_t,uint64_t);
  extern uint64_t sbreadcsr(uint64_t);
#else	/* normal case */
  #define WRITECSR(csr,val) *((volatile uint64_t *) (csr)) = (val)
  #define READCSR(csr) (*((volatile uint64_t *) (csr)))
#endif

/*  *********************************************************************
    *  JEDEC values 
    ********************************************************************* */


#define JEDEC_SDRAM_MRVAL_CAS15 0x52	/* 4-byte bursts, sequential, CAS 1.5 */
#define JEDEC_SDRAM_MRVAL_CAS2	0x22	/* 4-byte bursts, sequential, CAS 2 */
#define JEDEC_SDRAM_MRVAL_CAS25	0x62	/* 4-byte bursts, sequential, CAS 2.5 */
#define JEDEC_SDRAM_MRVAL_CAS3	0x32	/* 4-byte bursts, sequential, CAS 3 */
#define JEDEC_SDRAM_MRVAL_CAS35 0x72    /* 4-byte bursts, sequential, CAS 3.5 */
#define JEDEC_SDRAM_MRVAL_RESETDLL 0x100
#define JEDEC_SDRAM_EMRVAL	0x00

#define FCRAM_MRVAL		0x32
#define FCRAM_EMRVAL		0

#define SGRAM_MRVAL		0x32	/* 4-byte bursts, sequential, CAS 3 */
#define SGRAM_MRVAL_RESETDLL	0x400
#define SGRAM_EMRVAL		0x02

/* 
 * DECTO10THS(x) - this converts a BCD-style number found in 
 * JEDEC SPDs to a regular number.  So, 0x75 might mean "7.5ns"
 * and we convert this into tenths (75 decimal).  Many of the
 * calculations for the timing are done in terms of tenths of nanoseconds
 */

#define DECTO10THS(x) ((((x) >> 4)*10)+((x) & 0x0F))

/*  *********************************************************************
    *  Configuration parameter values
    ********************************************************************* */

#ifndef CFG_DRAM_MIN_tMEMCLK
#define CFG_DRAM_MIN_tMEMCLK	DRT10(8,0)	/* 8 ns, 125Mhz */
#endif

#ifndef CFG_DRAM_INTERLEAVE
#define CFG_DRAM_INTERLEAVE	0
#endif

#ifndef CFG_DRAM_SMBUS_CHANNEL
#define CFG_DRAM_SMBUS_CHANNEL	0
#endif

#ifndef CFG_DRAM_SMBUS_BASE
#define CFG_DRAM_SMBUS_BASE	0x54
#endif

#ifndef CFG_DRAM_ECC
#define CFG_DRAM_ECC		0
#endif

#ifndef CFG_DRAM_BLOCK_SIZE
#define CFG_DRAM_BLOCK_SIZE	32
#endif

#ifndef CFG_DRAM_CSINTERLEAVE
#define CFG_DRAM_CSINTERLEAVE	0
#endif

/*  *********************************************************************
    *  Memory region sizes (SB1250-specific)
    ********************************************************************* */

#define REGION0_LOC	0x0000
#define REGION0_SIZE	256

#define REGION1_LOC	0x0800
#define REGION1_SIZE	512

#define REGION2_LOC	0x0C00
#define REGION2_SIZE	256

#define REGION3_LOC	0x1000
#define REGION3_SIZE	(508*1024)		/* 508 GB! */

/*  *********************************************************************
    *  Global Data structure
    * 
    *  This is a hideous hack.  We're going to actually use "memory"
    *  before it is configured.  The L1 DCache will be clean before
    *  we get here, so we'll just locate this structure in memory 
    *  (at 0, for example) and "hope" we don't need to evict anything.
    *  If we keep the data below 256 cache lines, we'll only use one way
    *  of each cache line.  That's 8K, more than enough. 
    *
    *  This data structure needs to be used both for our data and the
    *  "C" stack, so be careful when you edit it!
    ********************************************************************* */

typedef struct csdata_s {		/* Geometry information from table */
    uint8_t rows;			/* or SMBbus */
    uint8_t cols;
    uint8_t banks;
    uint8_t flags;
   
    uint8_t spd_dramtype;		/* SPD[2] */
    uint8_t spd_tCK_25;			/* SPD[9]  tCK @ CAS 2.5 */
    uint8_t spd_tCK_20;			/* SPD[23] tCK @ CAS 2.0 */
    uint8_t spd_tCK_10;			/* SPD[25] tCK @ CAS 1.0 */
    uint8_t spd_rfsh;			/* SPD[12] Refresh Rate */
    uint8_t spd_caslatency;		/* SPD[18] CAS Latencies Supported */
    uint8_t spd_attributes;		/* SPD[21] Attributes */
    uint8_t spd_tRAS;			/* SPD[30] */
    uint8_t spd_tRP;			/* SPD[27] */
    uint8_t spd_tRRD;			/* SPD[28] */
    uint8_t spd_tRCD;			/* SPD[29] */
    uint8_t spd_tRFC;			/* SPD[42] */
    uint8_t spd_tRC;			/* SPD[41] */

} csdata_t;				/* total size: 16 bytes */

#define CS_PRESENT 1			/* chipsel is present (in use) */
#define CS_AUTO_TIMING 2		/* chipsel has timing information */

#define CS_CASLAT_10	0x20		/* upper four bits are the CAS latency */
#define CS_CASLAT_15	0x30		/* we selected.  bits 7..5 are the */
#define CS_CASLAT_20	0x40		/* whole number and bit 4 is the */
#define CS_CASLAT_25	0x50		/* fraction. */
#define CS_CASLAT_30	0x60
#define CS_CASLAT_MASK	0xF0
#define CS_CASLAT_SHIFT 4

typedef struct mcdata_s {		/* Information per memory controller */
    uint32_t cfgcsint; 			/* try to interleave this many CS bits */
    uint32_t csint;			/* # of chip select interleave bits */
    uint16_t mintmemclk;		/* minimum tMemClk */
    uint16_t roundtrip;			/* Round trip time from CLK to returned DQS at BCM1250 pin */
    uint32_t dramtype;			/* DRAM Type */
    uint32_t pagepolicy;		/* Page policy */
    uint32_t blksize;			/* Block Size */
    uint32_t flags;			/* ECC enabled */
    uint16_t tCK;			/* tCK for manual timing */
    uint16_t rfsh;			/* refresh rate for manual timing */
    uint64_t clkconfig;			/* default clock config */
    uint64_t mantiming;			/* manual timing */
    csdata_t csdata[MC_CHIPSELS];	/* Total size: 48 + 16*4 = 112 bytes */
} mcdata_t;

typedef struct initdata_s {
    uint64_t dscr[4];			/* Data Mover descriptor (one cache line)*/
    uint32_t flags;			/* various flags */
    uint32_t inuse;			/* indicates MC is in use */
    uint32_t pintbit;			/* port interleave bit */
    uint16_t firstchan;			/* first channel */
    uint16_t soctype;			/* SOC type */
    uint64_t ttlbytes;			/* total bytes */
    mcdata_t mc[MC_CHANNELS];		/* data per memory controller */
} initdata_t;				/* Total size: 56 + 112*2 = 280 bytes */

#define M_MCINIT_TRYPINTLV 1		/* Try to do port interleaving */
#define M_MCINIT_PINTLV	   2		/* Actually do port interleaving */



/*  *********************************************************************
    *  Configuration data structure
    ********************************************************************* */

#include "sb1250_draminit.h"

/*  *********************************************************************
    *  Initialized data
    *  
    *  WARNING WARNING WARNING!
    *  
    *  This module is *very magical*!   We are using the cache as
    *  SRAM, and we're running as relocatable code *before* the code
    *  is relocated and *before* the GP register is set up.
    *  
    *  Therefore, there should be NO data declared in the data
    *  segment - all data must be allocated in the .text segment
    *  and references to this data must be calculated by an inline
    *  assembly stub.  
    *  
    *  If you grep the disassembly of this file, you should not see
    *  ANY references to the GP register.
    ********************************************************************* */


#ifdef _MCSTANDALONE_NOISY_
static char *sb1250_rectypes[] = {"MCR_GLOBALS","MCR_CHCFG","MCR_TIMING",
				  "MCR_CLKCFG","MCR_GEOM","MCR_CFG",
				  "MCR_MANTIMING"};
#endif

/*  *********************************************************************
    *  Module Description
    *  
    *  This module attempts to initialize the DRAM controllers on
    *  the SB1250.  Each DRAM controller can control four chip
    *  selects, or two double-sided DDR SDRAM DIMMs.  Therefore, at
    *  most four DIMMs can be attached.
    *  
    *  We will assume that all of the DIMMs are connected to the same
    *  SMBUS serial bus, and are addressed sequentially starting from
    *  module 0.   The first two DIMMs will be assigned to memory
    *  controller #0 and the second two DIMMs will be assigned to
    *  memory controller #1.  
    *  
    *  There is one serial ROM per DIMM, and we will assume that the
    *  front and back of the DIMM are the same memory configuration.
    *  The first DIMM will be configured for CS0 and CS1, and the
    *  second DIMM will be configured for CS2 and CS3.   If the DIMM
    *  has only one side, it will be assigned to CS0 or CS2.
    *  
    *  No interleaving will be configured by this routine, but it
    *  should not be difficult to modify it should that be necessary.
    *  
    *  This entire routine needs to run from registers (no read/write
    *  data is allowed).
    *  
    *  The steps to initialize the DRAM controller are:
    *  
    *      * Read the SPD, verify DDR SDRAMs or FCRAMs
    *      * Obtain #rows, #cols, #banks, and module size
    *      * Calculate row, column, and bank masks
    *      * Calculate chip selects
    *      * Calculate timing register.  Note that we assume that
    *        all banks will use the same timing.
    *      * Repeat for each DRAM.
    *  
    *  DIMM0 -> MCTL0 : CS0, CS1	SPD Addr = 0x54
    *  DIMM1 -> MCTL0 : CS2, CS3	SPD Addr = 0x55
    *  DIMM2 -> MCTL1 : CS0, CS1	SPD Addr = 0x56
    *  DIMM3 -> MCTL1 : CS2, CS3	SPD Addr = 0x57
    *
    *  DRAM Controller registers are programmed in the following order:
    *  
    *  	   MC_CS_INTERLEAVE
    *  	   MC_CS_ATTR
    *  	   MC_TEST_DATA, MC_TEST_ECC
    *
    *  	   MC_CSx_ROWS, MC_CSx_COLS
    *      (repeated for each bank)
    *
    *  	   MC_CS_START, MC_CS_END
    *
    *  	   MC_CLOCK_CFG
    *      (delay)
    *  	   MC_TIMING
    *  	   MC_CONFIG
    *      (delay)
    *  	   MC_DRAMMODE
    *      (delay after each mode setting ??)
    *  
    *  Once the registers are initialized, the DRAM is activated by
    *  sending it the following sequence of commands:
    *  
    *       PRE (precharge)
    *       EMRS (extended mode register set)
    *       MRS (mode register set)
    *       PRE (precharge) 
    *       AR (auto-refresh)
    *       AR (auto-refresh again)
    *       MRS (mode register set)
    *  
    *  then wait 200 memory clock cycles without accessing DRAM.
    *  
    *  Following initialization, the ECC bits must be cleared.  This
    *  can be accomplished by disabling ECC checking on both memory
    *  controllers, and then zeroing all memory via the mapping
    *  in xkseg.
    ********************************************************************* */

/*  *********************************************************************
    *
    * Address Bit Assignment Algorithm:
    * 
    * Good performance can be achieved by taking the following steps
    * when assigning address bits to the row, column, and interleave
    * masks.  You will need to know the following:
    * 
    *    - The number of rows, columns, and banks on the memory devices
    *    - The block size (larger tends to be better for sequential
    *      access)
    *    - Whether you will interleave chip-selects
    *    - Whether you will be using both memory controllers and want
    *      to interleave between them
    * 
    * By choosing the masks carefully you can maximize the number of
    * open SDRAM banks and reduce access times for nearby and sequential
    * accesses.
    * 
    * The diagram below depicts a physical address and the order
    * that the bits should be placed into the masks.  Start with the 
    * least significant bit and assign bits to the row, column, bank,
    * and interleave registers in the following order:
    * 
    *         <------------Physical Address--------------->
    * Bits:	RRRRRRR..R  CCCCCCCC..C  NN  BB  P  CC  xx000
    * Step:	    7           6        5   4   3  2     1
    * 
    * Where:
    *     R = Row Address Bit     (MC_CSX_ROW register)
    *     C = Column Address Bit  (MC_CSX_COL register)
    *     N = Chip Select         (MC_CS_INTERLEAVE)    
    *                             (when interleaving via chip selects)
    *     B = Bank Bit            (MC_CSX_BA register)
    *     P = Port Select bit     (MC_CONFIG register)  
    *                             (when interleaving memory channels)
    *     x = Does not matter     (MC_CSX_COL register) 
    *                             (internally driven by controller)
    *     0 = must be zero
    * 
    * When an address bit is "assigned" it is set in one of the masks
    * in the MC_CSX_ROW, MC_CSX_COL, MC_CSX_BA, or MC_CS_INTERLEAVE
    * registers.
    * 
    * 
    * 1. The bottom 3 bits are ignored and should be set to zero.
    *    The next two bits are also ignored, but are considered
    *    to be column bits, so they should be taken from the 
    *    total column bits supported by the device.
    * 
    * 2. The next two bits are used for column interleave.  For
    *    32-byte blocks (and no column interleave), do not use 
    *    any column bits.  For 64-byte blocks, use one column 
    *    bit, and for 128 byte blocks, use two column bits.  Subtract 
    *    the column bits assigned in this step from the total.
    * 
    * 3. If you are using both memory controllers and wish to interleave
    *    between them, assign one bit for the controller interleave. The
    *    bit number is assigned in the MC_CONFIG register.
    * 
    * 4. These bits represent the bank bits on the memory device.
    *    If the device has 4 banks, assign 2 bits in the MC_CSX_BA 
    *    register.
    * 
    * 5. If you are interleaving via chip-selects, set one or two
    *    bits in the MC_CS_INTERLEAVE register for the bits that will
    *    be interleaved.
    * 
    * 6. The remaining column bits are assigned in the MC_CSX_COL
    *    register.
    * 
    * 7. The row bits are assigned in the MC_CSX_ROW register.
    * 
    ********************************************************************* */



/*  *********************************************************************
    *  sb1250_find_timingcs(mc)
    *  
    *  For a given memory controller, choose the chip select whose
    *  timing values will be used to base the TIMING and MCLOCK_CFG
    *  registers on.  
    *  
    *  Input parameters: 
    *  	   mc - memory controller
    *  	   
    *  Return value:
    *  	   chip select index, or -1 if no active chip selects.
    ********************************************************************* */


static int sb1250_find_timingcs(mcdata_t *mc)
{
    int idx;

    /* for now, the first one with data is the one we pick */

    for (idx = 0; idx < MC_CHIPSELS; idx++) {
	if (mc->csdata[idx].flags & CS_PRESENT) return idx;
	}

    return -1;
}

/*  *********************************************************************
    *  sb1250_auto_timing(mcidx,tdata)
    *  
    *  Program the memory controller's timing registers based on the
    *  timing information stored with the chip select data.  For DIMMs
    *  this information comes from the SPDs, otherwise it was entered
    *  from the datasheets into the tables in the init modules.
    *  
    *  Input parameters: 
    *  	   mcidx - memory controller index (0 or 1)
    *  	   tdata - a chip select data (csdata_t)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_auto_timing(int mcidx,mcdata_t *mc,csdata_t *tdata)
{
    unsigned int res;

    unsigned int plldiv;
    unsigned int clk_ratio;
    unsigned int refrate;
    unsigned int ref_freq;
    unsigned int caslatency;

    unsigned int spd_tCK_25;
    unsigned int spd_tCK_20;
    unsigned int spd_tCK_10;
    unsigned int tCpuClk;
    unsigned int tMemClk;

    unsigned int w2rIdle,r2wIdle,r2rIdle;
    unsigned int tCrD,tCrDh,tFIFO;
    unsigned int tCwD;
    unsigned int tRAS;
    unsigned int tRP,tRRD,tRCD,tRCw,tRCr,tCwCr,tRFC;

    uint64_t timing1;
    uint64_t mclkcfg;
    sbport_t base;
    uint64_t sysrev;

    /* Timing window variables */

    int addrSkew,dqiSkew,dqoSkew,clkDrive;
    int n01_open,n02_open,n12_open;
    int n01_close,n02_close,n12_close;
    int dqsArrival;
    int addrAdjust,dqiAdjust,dqoAdjust;
    int minDqsMargin;
    int dllScaleNum,dllScaleDenom,dllOffset;


    /*
     * We need our cpu clock for all sorts of things.
     */

    sysrev = READCSR(PHYS_TO_K1(A_SCD_SYSTEM_REVISION));
#if defined(_VERILOG_) || defined(_FUNCSIM_)
    plldiv = 16;		/* 800MHz CPU for RTL simulation */
#else
    plldiv = G_SYS_PLL_DIV(READCSR(PHYS_TO_K1(A_SCD_SYSTEM_CFG)));
#endif
    if (plldiv == 0) {
	plldiv = 6;
	}

    /*
     * Compute tCpuClk, in picoseconds to avoid rounding errors.
     *
     * Calculation:
     *     tCpuClk = 1/fCpuClk
     *             = 1/(100MHz * plldiv/2) 
     *             = 2/(100MHz*plldiv)
     *             = 2/(100*plldiv) us 
     *		   = 20/plldiv ns 
     *             = 2000000/plldiv 10ths of ns
     *
     * If SB1250_REFCLK is in MHz, then:
     *           2/(SB1250_REFCLK*plldiv) us 
     *         = 2000/(SB1250_REFCLK*plldiv) ns 
     *         = 2000000/(SB1250_REFCLK*plldiv) ps
     *
     * However, we want to round the result to the nearest integer,
     * so we double the numerator (to 4000000) to get one more bit
     * of precision in the quotient, then add one and scale it back down
     */

    /* tCpuClk is in picoseconds */
    tCpuClk = ((4000000/(SB1250_REFCLK*plldiv))+1)/2;

    spd_tCK_25 = DECTO10THS(tdata->spd_tCK_25);
    spd_tCK_20 = DECTO10THS(tdata->spd_tCK_20);
    spd_tCK_10 = DECTO10THS(tdata->spd_tCK_10);

    /* 
     * Compute the target tMemClk, in units of tenths of nanoseconds
     * to be similar to the JEDEC SPD values.  This will be
     *
     *     MAX(MIN_tMEMCLK,spd_tCK_25) 
     */

    tMemClk = spd_tCK_25;
    if (mc->mintmemclk > tMemClk) tMemClk = mc->mintmemclk;

    /* 
     * Now compute our clock ratio (the amount we'll divide tCpuClk by
     * to get as close as possible to tMemClk without exceeding it
     *
     * It's (tMemClk*100) here because tCpuClk is in picoseconds 
     */

    clk_ratio = ((tMemClk*100) + tCpuClk - 1) / tCpuClk;
    if (clk_ratio < 4) clk_ratio = 4;
    if (clk_ratio > 9) clk_ratio = 9;

    /*
     * BCM112x A1 parts do not function properly with MC ratio 5.
     * (This is fixed in A2 parts.)  On BCM112x before A2, When
     * that ratio would be used, back off to 6.
     */
    if ((SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1120 ||
         SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1125 ||
         SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1125H) &&
        G_SYS_REVISION(sysrev) < K_SYS_REVISION_BCM112x_A2 &&
	clk_ratio == 5) {
	clk_ratio = 6;
	}

    /* 
     * Now, recompute tMemClk using the new clk_ratio.  This gives us
     * the actual tMemClk that the memory controller will generate
     *
     * Calculation:
     *      fMemClk = SB1250_REFCLK * plldiv / (2 * clk_ratio) Mhz
     *
     *      tMemClk = 1/fMemClk us
     *              = (2 * clk_ratio) / (SB1250_REFCLK * plldiv) us
     *              = 10000 * (2 * clk_ratio) / (SB1250_REFCLK * plldiv) 0.1ns
     *
     * The resulting tMemClk is in tenths of nanoseconds so we
     * can compare it with the SPD values.  The x10000 converts
     * us to 0.1ns
     */

    tMemClk = (10000 * 2 * clk_ratio)/(SB1250_REFCLK * plldiv);

    /* Calculate the refresh rate */

    switch (tdata->spd_rfsh & JEDEC_RFSH_MASK) {
	case JEDEC_RFSH_64khz:	ref_freq = 64;  break;
	case JEDEC_RFSH_256khz:	ref_freq = 256; break;
	case JEDEC_RFSH_128khz:	ref_freq = 128; break;
	case JEDEC_RFSH_32khz:	ref_freq = 32;  break;
	case JEDEC_RFSH_8khz:	ref_freq = 16;  break;
	default: 		ref_freq = 8;   break;
	}

    /*
     * Compute the target refresh value, in Khz/16.  We know
     * the rate that the DIMMs expect (in Khz, above).  So we need
     * to calculate what the MemClk is divided by to get that value.
     * There is an internal divide-by-16 in the 1250 in the refresh
     * generation.
     *
     * Calculation:
     *     refrate = (plldiv/2)*SB1250_REFCLK*1000 Khz /(ref_freq*16*clk_ratio)
     */

    refrate = ((plldiv * SB1250_REFCLK * 1000 / 2) / (ref_freq*16*clk_ratio)) - 1;

    /*
     * Calculate CAS Latency in half cycles.  The low bit indicates
     * half a cycle, so 2 (0010) = 1 cycle and 3 (0011) = 1.5 cycles
     */

    res = tdata->spd_caslatency;
    if (res & JEDEC_CASLAT_35) caslatency = (3 << 1) + 1;	/* 3.5 */
    else if (res & JEDEC_CASLAT_30) caslatency = (3 << 1);	/* 3.0 */
    else if (res & JEDEC_CASLAT_25) caslatency = (2 << 1) + 1;	/* 2.5 */
    else if (res & JEDEC_CASLAT_20) caslatency = (2 << 1);	/* 2.0 */
    else if (res & JEDEC_CASLAT_15) caslatency = (1 << 1) + 1;	/* 1.5 */
    else caslatency = (1 << 1);					/* 1.0 */

    if ((spd_tCK_10 != 0) && (spd_tCK_10 <= tMemClk)) {
	caslatency -= (1 << 1);				/* subtract 1.0 */
	}
    else if ((spd_tCK_20 != 0) && (spd_tCK_20 <= tMemClk)) {
	caslatency -= 1;				/* subtract 0.5 */
	}

    /*
     * Store the CAS latency in the chip select info
     */
    
    tdata->flags &= ~CS_CASLAT_MASK;
    tdata->flags |= (((caslatency << CS_CASLAT_SHIFT)) & CS_CASLAT_MASK);
#ifdef _MCSTANDALONE_
    dram_cas_latency = caslatency;	
    dram_tMemClk = tMemClk;
#endif

    /*
     * Now, on to the timing parameters.
     */

    w2rIdle = 1;	/* Needs to be set on all parts. */
    r2rIdle = 0;

    /* ======================================================================== */

    /*
     * New "Window" calculations
     */

    n01_open = -SB1250_WINDOW_OPEN_OFFSET;
    n02_open = -SB1250_WINDOW_OPEN_OFFSET;
    n12_open = tMemClk/2 - SB1250_WINDOW_OPEN_OFFSET;
    n01_close = tMemClk - SB1250_CLOSE_01_OFFSET;
    n02_close = 3*tMemClk/2 - SB1250_CLOSE_02_OFFSET;
    n12_close = 7*tMemClk/4 - SB1250_CLOSE_12_OFFSET;
    minDqsMargin = SB1250_MIN_DQS_MARGIN;

    if (SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1250 &&
	G_SYS_REVISION(sysrev) >= K_SYS_REVISION_PASS1 &&
	G_SYS_REVISION(sysrev) < K_SYS_REVISION_PASS2) {
	/* pass1 bcm1250 */
	dllScaleNum = PASS1_DLL_SCALE_NUMERATOR;
	dllScaleDenom = PASS1_DLL_SCALE_DENOMINATOR;
	dllOffset = PASS1_DLL_OFFSET;
	}
    else {
	/* pass2+ BCM1250, or BCM112x */
	dllScaleNum = PASS2_DLL_SCALE_NUMERATOR;
	dllScaleDenom = PASS2_DLL_SCALE_DENOMINATOR;
	dllOffset = PASS2_DLL_OFFSET;
	}


    /*
     * Get fields out of the clock config register
     */

    dqiSkew =  (int) G_MC_DQI_SKEW(mc->clkconfig);
    dqoSkew =  (int) G_MC_DQO_SKEW(mc->clkconfig);
    addrSkew = (int) G_MC_ADDR_SKEW(mc->clkconfig);
    clkDrive = (int) G_MC_CLOCK_DRIVE(mc->clkconfig);

    /* 
     * get initial values for tCrD and dqsArrival
     */

    tCrD = (caslatency >> 1);
    dqsArrival = mc->roundtrip;
    if (caslatency & 1) {
	dqsArrival += tMemClk/2;
	}

    /*
     * need to adjust for settings of skew values.
     * can either add to dqsArrival or subtract from
     * all the windows.
     */

    addrAdjust = (addrSkew - 8) * ((int)tMemClk * dllScaleNum - dllOffset) / (8 * dllScaleDenom);
    dqiAdjust  = (dqiSkew - 8)  * ((int)tMemClk * dllScaleNum - dllOffset) / (8 * dllScaleDenom);
    dqsArrival += addrAdjust + dqiAdjust;

    /* for pass 2, dqoAdjust applies only to n12_Close */
    dqoAdjust  = (dqoSkew - 8)  * (tMemClk * dllScaleNum - dllOffset) / (8 * dllScaleDenom);
    n12_close += dqoAdjust;

    /* 
     * adjust window for clock drive strength 
     * Don't be tempted to turn this into an array.  It will break the
     * relocation stuff!
     */
    switch (clkDrive) {
	case 0:   dqsArrival += 10; break;
	case 1:   dqsArrival += 4;  break;
	case 2:   dqsArrival += 3;  break;
	case 3:   dqsArrival += 2;  break;
	case 4:   dqsArrival += 2;  break;
	case 5:   dqsArrival += 1;  break;
	case 6:   dqsArrival += 1;  break;
	case 7:   dqsArrival += 1;  break;
	case 8:   dqsArrival += 8;  break;
	case 9:   dqsArrival += 2;  break;
	case 0xa: dqsArrival += 1;  break;
	case 0xb: break;
	case 0xc: break;
	case 0xd: break;
	case 0xe: break;
	case 0xf: break;
	default:
	    /* shouldn't get here */
	    break;
	}

    while ((n02_close - dqsArrival < minDqsMargin) &&
	   (n12_close - dqsArrival < minDqsMargin)) {
	/* very late DQS arrival; shift latency by one tick */
#ifdef _MCSTANDALONE_NOISY_
	printf("DRAM: Very late DQS arrival, shift latency one tick\n");
#endif
	++tCrD;
	dqsArrival -= tMemClk;
	}

    if ((dqsArrival - n01_open  >= minDqsMargin) &&
	(n01_close - dqsArrival >= minDqsMargin)) {
	/* use n,0,1 */
	tCrDh = 0;
	tFIFO = 1;
#ifdef _MCSTANDALONE_NOISY_
	printf("DRAM: DQS arrival in n,0,1 window\n");
#endif
	} 
    else if ((dqsArrival - n02_open  >= minDqsMargin) &&
	     (n02_close - dqsArrival >= minDqsMargin)) {
	/* use n,0,2 */
	tCrDh = 0;
	tFIFO = 2;
#ifdef _MCSTANDALONE_NOISY_
	printf("DRAM: DQS arrival in n,0,2 window\n");
#endif
	} 
    else if ((dqsArrival - n12_open  >= minDqsMargin) &&
	     (n12_close - dqsArrival >= minDqsMargin)) {
	/* use n,1,2 */
	tCrDh = 1;
	tFIFO = 2;
#ifdef _MCSTANDALONE_NOISY_
	printf("DRAM: DQS arrival in n,1,2 window\n");
#endif
	} 
    else {
	/* 
	 * minDqsMargin is probably set too high 
	 * try using n,0,2
	 */
	tCrDh = 0;
	tFIFO = 2;
#ifdef _MCSTANDALONE_NOISY_
	printf("DRAM: Default: DQS arrival in n,0,2 window\n");
#endif
	}

    r2wIdle = ((tMemClk - dqsArrival) < SB1250_MIN_R2W_TIME);

    /*
     * Pass1 BCM112x parts do not function properly with
     * M_MC_r2wIDLE_TWOCYCLES clear, so we set r2wIdle here for them
     * so that that flag will be set later.
     */
    if ((SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1120 ||
         SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1125 ||
         SYS_SOC_TYPE(sysrev) == K_SYS_SOC_TYPE_BCM1125H) &&
	1 /* XXXCGD: When fixed, check revision! */) {
	r2wIdle = 1;
	}

    /* 
     * Above stuff just calculated tCrDh, tCrD, and tFIFO
     */

    /* ======================================================================== */

    /* Recompute tMemClk as a fixed-point 6.2 value */

    tMemClk = (4000 * 2 *  clk_ratio) / (SB1250_REFCLK * plldiv);

    tRAS = ( ((unsigned int)(tdata->spd_tRAS))*4 + tMemClk-1) / tMemClk;

    tRP =  ( ((unsigned int)(tdata->spd_tRP))    + tMemClk-1) / tMemClk;
    tRRD = ( ((unsigned int)(tdata->spd_tRRD))   + tMemClk-1) / tMemClk;
    tRCD = ( ((unsigned int)(tdata->spd_tRCD))   + tMemClk-1) / tMemClk;

    /* 
     * Check for registered DIMMs, or if we are "forcing" registered
     * DIMMs, as might be the case of regular unregistered DIMMs
     * behind an external register.
     */

    switch (mc->dramtype) {
	case FCRAM:
	    /* For FCRAMs, tCwD is always caslatency - 1  */
	    tCwD = (caslatency >> 1) - 1; 
	    tRCD = 1;		/* always 1 for FCRAM */
	    tRP = 1;		/* always 1 for FCRAM */
	    break;
	default:
	    /* Otherwise calculate based on registered attribute */
	    if ((tdata->spd_attributes & JEDEC_ATTRIB_REG) ||
		(mc->flags & MCFLG_FORCEREG)) {  	/* registered DIMM */
		tCwD = 2; 
		tCrD++;
		}
	    else {			/* standard unbuffered DIMM */
		tCwD = 1;
		}
	    break;
	}

    /*
     * Calculate the rest of the timing parameters based on above values.
     */

    if (tdata->spd_tRFC == 0) {
	tRFC = 1 + tRP + tRAS;
	}
    else {
	tRFC = ( ((unsigned int) tdata->spd_tRFC)*4 + tMemClk-1) / tMemClk;
	}

    if (tRFC > 15) tRFC = 15;

    if (tdata->spd_tRC == 0) {
	tRCw = (tRAS + tRP + 2) - 1;
	tRCr = (tRAS + tRP) - 1;
	}
    else {
	tRCw = ( (((unsigned int)(tdata->spd_tRC))*4 + tMemClk-1) / tMemClk) + 2 - 1;
	tRCr = ( (((unsigned int)(tdata->spd_tRC))*4 + tMemClk-1) / tMemClk) - 1;
	}

    tCwCr = tRCw + 1 - (tRCD + tCwD + 2 + 1);

    /*
     * Finally, put it all together in the timing register.
     */


    timing1 = V_MC_tRCD(tRCD) |
	V_MC_tCrD(tCrD) |
	(tCrDh ? M_tCrDh : 0) |
	V_MC_tRP(tRP) |
	V_MC_tRRD(tRRD) |
	V_MC_tRCw(tRCw) |
	V_MC_tRCr(tRCr) |
	V_MC_tCwCr(tCwCr) |
	V_MC_tRFC(tRFC) |
	V_MC_tFIFO(tFIFO) |
	V_MC_tCwD(tCwD) |
	(w2rIdle ? M_MC_w2rIDLE_TWOCYCLES : 0) |
	(r2wIdle ? M_MC_r2wIDLE_TWOCYCLES : 0) |
	(r2rIdle ? M_MC_r2rIDLE_TWOCYCLES : 0);

    mclkcfg = V_MC_CLK_RATIO(clk_ratio) |
	V_MC_REF_RATE(refrate);

    /* Merge in drive strengths from the MC structure */
    mclkcfg |= mc->clkconfig;

    base = PHYS_TO_K1(A_MC_BASE(mcidx));
    WRITECSR(base+R_MC_TIMING1,timing1);

#ifdef _VERILOG_
    /* Smash in some defaults for Verilog simulation */
    mclkcfg &= ~(M_MC_CLK_RATIO | M_MC_DLL_DEFAULT | M_MC_REF_RATE);
    mclkcfg |= V_MC_CLK_RATIO_3X | V_MC_REF_RATE(K_MC_REF_RATE_200MHz) | 
	V_MC_DLL_DEFAULT(0x18);
#endif

    WRITECSR(base+R_MC_MCLK_CFG,mclkcfg);

}


/*  *********************************************************************
    *  SB1250_MANUAL_TIMING(mcidx,mc)
    *  
    *  Program the timing registers, for the case of user-specified
    *  timing parameters (don't calculate values based on datasheet
    *  values, just stuff the info into the MC registers)
    *  
    *  Input parameters: 
    *  	   mcidx - memory controller index
    *  	   mc - memory controller data
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_manual_timing(int mcidx,mcdata_t *mc)
{
    unsigned int plldiv;
    unsigned int clk_ratio;
    unsigned int refrate;
    unsigned int ref_freq;
    unsigned int tCpuClk;
    unsigned int tMemClk;

    uint64_t timing1;
    uint64_t mclkcfg;

    sbport_t base;

    /*
     * We need our cpu clock for all sorts of things.
     */

#if defined(_VERILOG_) || defined(_FUNCSIM_)
    plldiv = 16;		/* 800MHz CPU for RTL simulation */
#else
    plldiv = G_SYS_PLL_DIV(READCSR(PHYS_TO_K1(A_SCD_SYSTEM_CFG)));
#endif
    if (plldiv == 0) {
	plldiv = 6;
	}

    /* See comments in auto_timing for details */
    tCpuClk = 2000000/(SB1250_REFCLK*plldiv);	/* tCpuClk is in picoseconds */

    /* Compute MAX(MIN_tMEMCLK,spd_tCK_25) */
    tMemClk = DECTO10THS(mc->tCK);
    if (mc->mintmemclk > tMemClk) tMemClk = mc->mintmemclk;

    clk_ratio = ((tMemClk*100) + tCpuClk - 1) / tCpuClk;
    if (clk_ratio < 4) clk_ratio = 4;
    if (clk_ratio > 9) clk_ratio = 9;

    /* recompute tMemClk using the new clk_ratio */

    tMemClk = (10000 * 2 * clk_ratio)/(SB1250_REFCLK * plldiv);

    /* Calculate the refresh rate */

    switch (mc->rfsh & JEDEC_RFSH_MASK) {
	case JEDEC_RFSH_64khz:	ref_freq = 64;  break;
	case JEDEC_RFSH_256khz:	ref_freq = 256; break;
	case JEDEC_RFSH_128khz:	ref_freq = 128; break;
	case JEDEC_RFSH_32khz:	ref_freq = 32;  break;
	case JEDEC_RFSH_8khz:	ref_freq = 16;  break;
	default: 		ref_freq = 8;   break;
	}

    refrate = ((plldiv * SB1250_REFCLK * 1000 / 2) / (ref_freq*16*clk_ratio)) - 1;

    timing1 = mc->mantiming;
    mclkcfg = V_MC_CLK_RATIO(clk_ratio) |
	V_MC_REF_RATE(refrate);

    /* Merge in drive strengths from the MC structure */
    mclkcfg |= mc->clkconfig;

    base = PHYS_TO_K1(A_MC_BASE(mcidx));
    WRITECSR(base+R_MC_TIMING1,timing1);

#ifdef _VERILOG_
    /* Smash in some defaults for Verilog simulation */
    mclkcfg &= ~(M_MC_CLK_RATIO | M_MC_DLL_DEFAULT | M_MC_REF_RATE);
    mclkcfg |= V_MC_CLK_RATIO_3X | V_MC_REF_RATE(K_MC_REF_RATE_200MHz) | 
	V_MC_DLL_DEFAULT(0x18);
#endif

    WRITECSR(base+R_MC_MCLK_CFG,mclkcfg);

}

	

/*  *********************************************************************
    *  Default DRAM init table
    * 
    *  This is just here to make SB1250 BSPs easier to write.
    *  If you've hooked up standard JEDEC SDRAMs in a standard
    *  way with all your SPD ROMs on one SMBus channel,
    *  This table is for you.
    * 
    *  Otherwise, copy it into your board_init.S file and 
    *  modify it, and return a pointer to the table from
    *  the board_draminfo routine.
    *
    *  (See the CFE manual for more details)
    ********************************************************************* */

#if !defined(_MCSTANDALONE_)		    /* no tables in the non-CFE, non-MIPS version */

#ifdef _VERILOG_
static const mc_initrec_t draminittab_11xx[5] __attribute__ ((section(".text"))) = {

    /*
     * Globals: No interleaving
     */

    DRAM_GLOBALS(MC_NOPORTINTLV),

    /*
     * Memory channel 0:  manually configure for verilog runs
     * Configure chip select 0.
     */

    DRAM_CHAN_CFG(MC_CHAN0, 80, JEDEC, CASCHECK, BLKSIZE32, NOCSINTLV, ECCDISABLE, 0),

    DRAM_CS_GEOM(MC_CS0, 12, 8, 2),
    DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_64khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0),

    DRAM_EOT
};

static const mc_initrec_t draminittab_12xx[5] __attribute__ ((section(".text"))) = {

    /*
     * Globals: No interleaving
     */

    DRAM_GLOBALS(MC_NOPORTINTLV),

    /*
     * Memory channel 0:  manually configure for verilog runs
     * Configure chip select 0.
     */

    DRAM_CHAN_CFG(MC_CHAN0, 80, JEDEC, CASCHECK, BLKSIZE32, NOCSINTLV, ECCDISABLE, 0),

    DRAM_CS_GEOM(MC_CS0, 12, 8, 2),
    DRAM_CS_TIMING(DRT10(7,5), JEDEC_RFSH_64khz, JEDEC_CASLAT_25, 0,  45, DRT4(20,0), DRT4(15,0),  DRT4(20,0),  0, 0),

    DRAM_EOT
};


#else
#define DEVADDR (CFG_DRAM_SMBUS_BASE)
#define DEFCHAN (CFG_DRAM_SMBUS_CHANNEL)
static const mc_initrec_t draminittab_12xx[8] __attribute__ ((section(".text"))) = {

    /*
     * Global data: Interleave mode from bsp_config.h
     */

    DRAM_GLOBALS(CFG_DRAM_INTERLEAVE),	   		/* do port interleaving if possible */

    /*
     * Memory channel 0: Configure via SMBUS, Automatic Timing
     * Assumes SMBus device numbers are arranged such
     * that the first two addresses are CS0,1 and CS2,3 on MC0
     * and the second two addresses are CS0,1 and CS2,3 on MC1
     */

    DRAM_CHAN_CFG(MC_CHAN0, CFG_DRAM_MIN_tMEMCLK, DRAM_TYPE_SPD, CASCHECK, CFG_DRAM_BLOCK_SIZE, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0),

    DRAM_CS_SPD(MC_CS0, 0, DEFCHAN, DEVADDR+0),	
    DRAM_CS_SPD(MC_CS2, 0, DEFCHAN, DEVADDR+1),

    /*
     * Memory channel 1: Configure via SMBUS
     */

    DRAM_CHAN_CFG(MC_CHAN1, CFG_DRAM_MIN_tMEMCLK, DRAM_TYPE_SPD, CASCHECK, CFG_DRAM_BLOCK_SIZE, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0),

    DRAM_CS_SPD(MC_CS0, 0, DEFCHAN, DEVADDR+2),	
    DRAM_CS_SPD(MC_CS2, 0, DEFCHAN, DEVADDR+3),

   /*
    * End of Table
    */

    DRAM_EOT

};

static const mc_initrec_t draminittab_11xx[5] __attribute__ ((section(".text"))) = {

    /*
     * Global data: Interleave mode from bsp_config.h
     */

    DRAM_GLOBALS(0),	   		/* no port interleaving on 11xx */

    /*
     * Memory channel 1: Configure via SMBUS
     */

    DRAM_CHAN_CFG(MC_CHAN1, CFG_DRAM_MIN_tMEMCLK, DRAM_TYPE_SPD, CASCHECK, CFG_DRAM_BLOCK_SIZE, CFG_DRAM_CSINTERLEAVE, CFG_DRAM_ECC, 0),

    DRAM_CS_SPD(MC_CS0, 0, DEFCHAN, DEVADDR+0),	
    DRAM_CS_SPD(MC_CS2, 0, DEFCHAN, DEVADDR+1),

   /*
    * End of Table
    */

    DRAM_EOT

};
#endif

#endif


/*  *********************************************************************
    *  SB1250_SMBUS_INIT()
    *  
    *  Initialize SMBUS channel 
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel number, 0 or 1
    *  	   
    *  Return value:
    *  	   smbus_base - KSEG1 address of SMBus channel
    *  
    *  Registers used:
    *  	   tmp0
    ********************************************************************* */

static sbport_t sb1250_smbus_init(int chan)
{
    sbport_t base;

    base = PHYS_TO_K1(A_SMB_BASE(chan));

    WRITECSR(base+R_SMB_FREQ,K_SMB_FREQ_100KHZ);
    WRITECSR(base+R_SMB_CONTROL,0);

    return base;
}
	

/*  *********************************************************************
    *  SB1250_SMBUS_WAITREADY()
    *  
    *  Wait for SMBUS channel to be ready.
    *  
    *  Input parameters: 
    *  	   smbus_base - SMBus channel base (K1seg addr)
    *  	   
    *  Return value:
    *  	   ret0 - 0 if no error occured, else -1
    *  
    *  Registers used:
    *  	   tmp0,tmp1
    ********************************************************************* */

static int sb1250_smbus_waitready(sbport_t base)
{
    uint64_t status;

    /*	
     * Wait for busy bit to clear
     */

    for (;;) {
	status = READCSR(base+R_SMB_STATUS);
	if (!(status & M_SMB_BUSY)) break;
	}

    /*
     * Isolate error bit and clear error status
     */

    status &= M_SMB_ERROR;
    WRITECSR(base+R_SMB_STATUS,status);

    /*
     * Return status
     */

    return (status) ? -1 : 0;
}



/*  *********************************************************************
    *  SB1250_SMBUS_READBYTE()
    *  
    *  Read a byte from a serial ROM attached to an SMBus channel
    *  
    *  Input parameters: 
    *      base - SMBus channel base address (K1seg addr)
    *  	   dev - address of device on SMBUS
    *      offset - address of byte within device on SMBUS
    *  	   
    *  Return value:
    *  	   byte from device (-1 indicates an error)
    ********************************************************************* */


static int sb1250_smbus_readbyte(sbport_t base,unsigned int dev,unsigned int offset)
{
    int res;

    /*
     * Wait for channel to be ready
     */

    res = sb1250_smbus_waitready(base);
    if (res < 0) return res;

    /*
     * Set up a READ BYTE command.  This command has no associated
     * data field, the command code is the data 
     */

    WRITECSR(base+R_SMB_CMD,offset);
    WRITECSR(base+R_SMB_START,dev | V_SMB_TT(K_SMB_TT_CMD_RD1BYTE));

    /* 
     * Wait for the command to complete
     */

    res = sb1250_smbus_waitready(base);
    if (res < 0) return res;

    /*
     * Return the data byte
     */

    return (int) ((READCSR(base+R_SMB_DATA)) & 0xFF);
}


/*  *********************************************************************
    *  SB1250_DRAM_GETINFO
    *  
    *  Process a single init table entry and move data into the
    *  memory controller's data structure.
    *  
    *  Input parameters: 
    *	   smbase - points to base of SMbus device to read from
    *  	   mc - memory controller data
    *      init - pointer to current user init table entry
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_dram_getinfo(unsigned int smbchan,
				unsigned int smbdev,
				mcdata_t *mc,
				int chipsel)

{
    int res;
    unsigned char spd[JEDEC_SPD_MAX];
    int idx;
    csdata_t *cs = &(mc->csdata[chipsel]);
    sbport_t smbase;

    smbase = sb1250_smbus_init(smbchan);

    /*
     * Read just the memory type to see if the RAM is present.
     */

    res = sb1250_smbus_readbyte(smbase,smbdev,JEDEC_SPD_MEMTYPE);

    if ((res < 0) || ((res != JEDEC_MEMTYPE_DDRSDRAM) && 
		      (res != JEDEC_MEMTYPE_DDRSDRAM2) &&
	              (res != SPD_MEMTYPE_FCRAM))) {
	return;			/* invalid or no memory installed */
	}

    /*
     * Now go back and read everything.
     */

    res = 0;
    for (idx = 0; idx < JEDEC_SPD_MAX; idx++) {
	res = sb1250_smbus_readbyte(smbase,smbdev,idx);
	if (res < 0) break;
	spd[idx] = res;
	}

    if (res < 0) return;		/* some SMBus error */


    cs->rows = spd[JEDEC_SPD_ROWS];
    cs->cols = spd[JEDEC_SPD_COLS];

    /*
     * Determine how many bits the banks represent.  Unlike
     * the rows/columns, the bank byte says how *many* banks
     * there are, not how many bits represent banks
     */

    switch (spd[JEDEC_SPD_BANKS]) {
	case 2:					/* 2 banks = 1 bits */
	    cs->banks = 1;
	    break;

	case 4:					/* 4 banks = 2 bits */
	    cs->banks = 2;
	    break;

	case 8:					/* 8 banks = 3 bits */
	    cs->banks = 3;
	    break;

	case 16:				/* 16 banks = 4 bits */
	    cs->banks = 4;
	    break;

	default:				/* invalid bank count */
	    return;
	}


    /*
     * Read timing parameters from the DIMM.  By this time we kind of trust
     */

    cs->spd_dramtype   = spd[JEDEC_SPD_MEMTYPE];
    cs->spd_tCK_25     = spd[JEDEC_SPD_tCK25];
    cs->spd_tCK_20     = spd[JEDEC_SPD_tCK20];
    cs->spd_tCK_10     = spd[JEDEC_SPD_tCK10];
    cs->spd_rfsh       = spd[JEDEC_SPD_RFSH];
    cs->spd_caslatency = spd[JEDEC_SPD_CASLATENCIES];
    cs->spd_attributes = spd[JEDEC_SPD_ATTRIBUTES];
    cs->spd_tRAS       = spd[JEDEC_SPD_tRAS];
    cs->spd_tRP        = spd[JEDEC_SPD_tRP];
    cs->spd_tRRD       = spd[JEDEC_SPD_tRRD];
    cs->spd_tRCD       = spd[JEDEC_SPD_tRCD];
    cs->spd_tRFC       = spd[JEDEC_SPD_tRFC];
    cs->spd_tRC        = spd[JEDEC_SPD_tRC];

    /*
     * Okay, we got all the required data.  mark this CS present.
     */

    cs->flags = CS_PRESENT | CS_AUTO_TIMING;

    /*
     * If the module width is not 72 for any DIMM, disable ECC for this
     * channel.  All DIMMs must support ECC for us to enable it.
     */

    if (spd[JEDEC_SPD_WIDTH] != 72) mc->flags &= ~MCFLG_ECC_ENABLE;

    /*
     * If it was a double-sided DIMM, also mark the odd chip select
     * present.
     */

    if ((spd[JEDEC_SPD_SIDES] == 2) && !(mc->flags & MCFLG_BIGMEM)) {
	csdata_t *oddcs = &(mc->csdata[chipsel | 1]);

	oddcs->rows  = cs->rows;
	oddcs->cols  = cs->cols;
	oddcs->banks = cs->banks;
	oddcs->flags = CS_PRESENT;

	oddcs->spd_dramtype   = spd[JEDEC_SPD_MEMTYPE];
	oddcs->spd_tCK_25     = spd[JEDEC_SPD_tCK25];
	oddcs->spd_tCK_20     = spd[JEDEC_SPD_tCK20];
	oddcs->spd_tCK_10     = spd[JEDEC_SPD_tCK10];
	oddcs->spd_rfsh       = spd[JEDEC_SPD_RFSH];
	oddcs->spd_caslatency = spd[JEDEC_SPD_CASLATENCIES];
	oddcs->spd_attributes = spd[JEDEC_SPD_ATTRIBUTES];
	oddcs->spd_tRAS       = spd[JEDEC_SPD_tRAS];
	oddcs->spd_tRP        = spd[JEDEC_SPD_tRP];
	oddcs->spd_tRRD       = spd[JEDEC_SPD_tRRD];
	oddcs->spd_tRCD       = spd[JEDEC_SPD_tRCD];
	oddcs->spd_tRFC       = spd[JEDEC_SPD_tRFC];
	oddcs->spd_tRC        = spd[JEDEC_SPD_tRC];
	}

}


/*  *********************************************************************
    *  SB1250_DRAM_READPARAMS(d,init)
    *  
    *  Read all the parameters from the user parameter table and
    *  digest them into our local data structure.  This routine basically
    *  walks the table and calls the routine above to handle each
    *  entry.
    *  
    *  Input parameters: 
    *  	   d - our data structure (our RAM data)
    *  	   init - pointer to user config table
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_dram_readparams(initdata_t *d,const draminittab_t *init)
{
    mcdata_t *mc;
    csdata_t *cs;
    uint64_t sysrev;

    sysrev = READCSR(PHYS_TO_K1(A_SCD_SYSTEM_REVISION));

    /*
     * Assume we're starting on the first channel.  We should have a CHCFG record
     * to set the initial channel number, this is just in case.
     */
    mc = &(d->mc[d->firstchan]);

    /* Default clock config unless overridden */
    if (G_SYS_REVISION(sysrev) >= K_SYS_REVISION_PASS1 &&
	G_SYS_REVISION(sysrev) < K_SYS_REVISION_PASS2) {
	/* pass1 */
	mc->clkconfig = V_MC_CLKCONFIG_VALUE_PASS1;
	}
    else {
	/* pass2 */
	mc->clkconfig = V_MC_CLKCONFIG_VALUE;
	}

    cs = &(mc->csdata[0]);

    while (init->mcr.mcr_type != MCR_EOT) {

#ifdef _MCSTANDALONE_NOISY_
	printf("DRAM: Processing record '%s'\n",sb1250_rectypes[init->mcr.mcr_type]);
#endif

	switch (init->mcr.mcr_type) {

	    case MCR_GLOBALS:
		if (init->gbl.gbl_intlv_ch) d->flags |= M_MCINIT_TRYPINTLV;
		break;

	    case MCR_CHCFG:
		mc = &(d->mc[init->cfg.cfg_chan]);
		mc->mintmemclk = DECTO10THS(init->cfg.cfg_mintmemclk);
		mc->dramtype   = init->cfg.cfg_dramtype;
		mc->pagepolicy = init->cfg.cfg_pagepolicy;
		mc->blksize    = init->cfg.cfg_blksize;
		mc->cfgcsint   = init->cfg.cfg_intlv_cs;

		/* Default clock config unless overridden */
		if (G_SYS_REVISION(sysrev) >= K_SYS_REVISION_PASS1 &&
		    G_SYS_REVISION(sysrev) < K_SYS_REVISION_PASS2) {
		    /* pass1 */
		    mc->clkconfig = V_MC_CLKCONFIG_VALUE_PASS1;
		    }
		else {
		    /* pass2 */
		    mc->clkconfig = V_MC_CLKCONFIG_VALUE;
		    }

		mc->flags = (init->cfg.cfg_ecc & MCFLG_ECC_ENABLE) |
		    (init->cfg.cfg_flags & (MCFLG_FORCEREG | MCFLG_BIGMEM));
		mc->roundtrip  = DECTO10THS(init->cfg.cfg_roundtrip);
		if (mc->roundtrip == 0 && mc->dramtype != DRAM_TYPE_SPD) {
		    /* 
		     * Only set default roundtrip if mem type is specified, else wait
		     * to get type from SPD 
		     */
		    mc->roundtrip = (mc->dramtype == FCRAM) ?
			DEFAULT_MEMORY_ROUNDTRIP_TIME_FCRAM : DEFAULT_MEMORY_ROUNDTRIP_TIME;
		    }
		if (mc->dramtype == FCRAM) mc->pagepolicy = CLOSED; 	/*FCRAM must be closed page policy*/
		cs = &(mc->csdata[0]);
		break;

	    case MCR_TIMING:
		cs->spd_tCK_25 = init->tmg.tmg_tCK;
		cs->spd_tCK_20 = 0;
		cs->spd_tCK_10 = 0;
		cs->spd_rfsh = init->tmg.tmg_rfsh;
		cs->spd_caslatency = init->tmg.tmg_caslatency;
		cs->spd_attributes = init->tmg.tmg_attributes;
		cs->spd_tRAS = init->tmg.tmg_tRAS;
		cs->spd_tRP = init->tmg.tmg_tRP;
		cs->spd_tRRD = init->tmg.tmg_tRRD;
		cs->spd_tRCD = init->tmg.tmg_tRCD;
		cs->spd_tRFC = init->tmg.tmg_tRFC;
		cs->spd_tRC = init->tmg.tmg_tRC;
		break;
		
	    case MCR_CLKCFG:
		mc->clkconfig = 
		    V_MC_ADDR_SKEW((uint64_t)(init->clk.clk_addrskew)) | 
		    V_MC_DQO_SKEW((uint64_t)(init->clk.clk_dqoskew)) | 
		    V_MC_DQI_SKEW((uint64_t)(init->clk.clk_dqiskew)) | 
		    V_MC_ADDR_DRIVE((uint64_t)(init->clk.clk_addrdrive)) | 
		    V_MC_DATA_DRIVE((uint64_t)(init->clk.clk_datadrive)) | 
		    V_MC_CLOCK_DRIVE((uint64_t)(init->clk.clk_clkdrive)); 
		break;

	    case MCR_GEOM:
		cs = &(mc->csdata[init->geom.geom_csel]);
		cs->rows = init->geom.geom_rows;
		cs->cols = init->geom.geom_cols;
		cs->banks = init->geom.geom_banks;
		cs->flags |= CS_PRESENT;	
		break;

	    case MCR_SPD:
		cs = &(mc->csdata[init->spd.spd_csel]);
		sb1250_dram_getinfo(init->spd.spd_smbuschan,
				    init->spd.spd_smbusdev,
				    mc,
				    init->spd.spd_csel);

		if (mc->dramtype == DRAM_TYPE_SPD) {
		    /* Use the DRAM type we get from the SPD */
		    if (cs->spd_dramtype == SPD_MEMTYPE_FCRAM){
			mc->dramtype = FCRAM;
			mc->pagepolicy = CLOSED;
			if (mc->roundtrip == 0) mc->roundtrip = DEFAULT_MEMORY_ROUNDTRIP_TIME_FCRAM;
			}
		    else {
			mc->dramtype = JEDEC;
			if (mc->roundtrip == 0) mc->roundtrip = DEFAULT_MEMORY_ROUNDTRIP_TIME;		
			}
		    }
		mc->rfsh = cs->spd_rfsh;
		break;

	    case MCR_MANTIMING:
		/* Manual timing - pick record up as bytes because we cannot
		   guarantee the alignment of the "mtm_timing" field in our
		   structure -- each row is 12 bytes, not good */
		mc->rfsh = (uint16_t) init->mtm.mtm_rfsh;	/* units: JEDEC refresh value */
		mc->tCK  = (uint16_t) init->mtm.mtm_tCK;	/* units: BCD, like SPD */
		mc->mantiming =
		    (((uint64_t) init->mtm.mtm_timing[0]) << 56) |
		    (((uint64_t) init->mtm.mtm_timing[1]) << 48) |
		    (((uint64_t) init->mtm.mtm_timing[2]) << 40) |
		    (((uint64_t) init->mtm.mtm_timing[3]) << 32) |
		    (((uint64_t) init->mtm.mtm_timing[4]) << 24) |
		    (((uint64_t) init->mtm.mtm_timing[5]) << 16) |
		    (((uint64_t) init->mtm.mtm_timing[6]) << 8) |
		    (((uint64_t) init->mtm.mtm_timing[7]) << 0);
		break;

	    default:
		break;
	    }

	init++;
	}

    /*
     * Okay, now we've internalized all the data from the SPDs
     * and/or the init table.
     */
    
}



/*  *********************************************************************
    *  SB1250_DRAMINIT_DELAY
    *  
    *  This little routine delays at least 200 microseconds.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing.
    *  	   
    *  Registers used:
    *  	   tmp0,tmp1
    ********************************************************************* */

/* 200 microseconds = 5KHz, so delay 1GHz/5Khz = 200,000 cycles */

#ifdef _FASTEMUL_
#define DRAMINIT_DELAY_CNT	"50"
#else
#define DRAMINIT_DELAY_CNT	 "(1000000000/5000)"
#endif

#if defined(_MCSTANDALONE_)
#define DRAMINIT_DELAY()		/* not running on a 1250, no delays */
#else
#define DRAMINIT_DELAY() sb1250_draminit_delay()

static void sb1250_draminit_delay(void)
{
    __asm("     li $9," DRAMINIT_DELAY_CNT " ; "
	  "     mtc0 $0,$9 ; "
	  "1:   mfc0 $8,$9 ; "
	  "     .set push ; .set mips64 ; ssnop ; ssnop ; .set pop ;"
	  "     blt $8,$9,1b ;");
}
#endif




/*  *********************************************************************
    *  MAKEDRAMMASK(dest,width,pos)
    *  
    *  Create a 64-bit mask for the DRAM config registers
    *  
    *  Input parameters: 
    *  	   width - number of '1' bits to set
    *  	   pos - position (from the right) of the first '1' bit
    *  	   
    *  Return value:
    *  	   mask with specified with at specified position 
    ********************************************************************* */

#define MAKEDRAMMASK(width,pos) _SB_MAKEMASK(width,pos)


/*  *********************************************************************
    *  SB1250_JEDEC_INITCMDS
    *  
    *  Issue the sequence of DRAM init commands (JEDEC SDRAMs)
    *  
    *  Input parameters: 
    *      mcnum - memory controller index (0/1)
    *  	   mc - pointer to data for this memory controller
    *	   csel - which chip select to send init commands for
    *      lmbank - for largemem systems, the cs qualifiers to be
    *		    output on CS[2:3]
    *      tdata - chip select to use as a template for timing data
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_jedec_initcmds(int mcnum,mcdata_t *mc,int csel,
				  int lmbank,csdata_t *tdata)
{
    uint64_t csmask;
    sbport_t cmd;
    sbport_t mode;
    uint64_t modebits;
    uint64_t casbits;

    csmask = M_MC_CS0 << csel;		/* convert chip select # to mask */

    if (mc->flags & MCFLG_BIGMEM) {
	/* 
	 * so that the banks will all get their precharge signals,
	 * put the CS qualifiers out on CS[2:3].
	 */	   
	csmask |= (uint64_t)(lmbank << 6);
	}

    cmd  = (sbport_t) PHYS_TO_K1(A_MC_REGISTER(mcnum,R_MC_DRAMCMD));
    mode = (sbport_t) PHYS_TO_K1(A_MC_REGISTER(mcnum,R_MC_DRAMMODE));

    /*
     * Using the data in the timing template, figure out which
     * CAS latency command to issue.
     */

    switch (tdata->flags & CS_CASLAT_MASK) {
	case CS_CASLAT_30:
	    casbits = V_MC_MODE(JEDEC_SDRAM_MRVAL_CAS3);
	    break;
	default:
	case CS_CASLAT_25:
	    casbits = V_MC_MODE(JEDEC_SDRAM_MRVAL_CAS25);
	    break;
	case CS_CASLAT_20:
	    casbits = V_MC_MODE(JEDEC_SDRAM_MRVAL_CAS2);
	    break;
	case CS_CASLAT_15:
	    casbits = V_MC_MODE(JEDEC_SDRAM_MRVAL_CAS15);
	    break;
	}

    /*
     * Set up for doing mode register writes to the SDRAMs
     * 
     * First time, we set bit 8 to reset the DLL
     */

    modebits = V_MC_EMODE(JEDEC_SDRAM_EMRVAL) |
	     V_MC_MODE(JEDEC_SDRAM_MRVAL_RESETDLL) | 
	     V_MC_DRAM_TYPE(JEDEC);

    WRITECSR(mode,modebits | casbits);

    WRITECSR(cmd,csmask | V_MC_COMMAND_CLRPWRDN);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_PRE);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_EMRS);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_MRS);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_PRE);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_AR);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_AR);
    DRAMINIT_DELAY();

    /*
     * This time, clear bit 8 to start the DLL
     */

    modebits = V_MC_EMODE(JEDEC_SDRAM_EMRVAL) |
	V_MC_DRAM_TYPE(JEDEC);

    WRITECSR(mode,modebits | casbits);
    WRITECSR(cmd,csmask | V_MC_COMMAND_MRS);
    DRAMINIT_DELAY();

}

/*  *********************************************************************
    *  SB1250_SGRAM_INITCMDS
    *  
    *  Issue the sequence of DRAM init commands. (SGRAMs)
    *  Note: this routine does not support "big memory" (external decode)
    *  
    *  Input parameters: 
    *      mcnum - memory controller index (0/1)
    *  	   mc - pointer to data for this memory controller
    *	   csel - which chip select to send init commands for
    *      tdata - chip select to use as a template for timing data
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_sgram_initcmds(int mcnum,mcdata_t *mc,int csel,csdata_t *tdata)
{
    uint64_t csmask;
    sbport_t cmd;
    sbport_t mode;

    csmask = M_MC_CS0 << csel;		/* convert chip select # to mask */
    cmd  = (sbport_t) PHYS_TO_K1(A_MC_REGISTER(mcnum,R_MC_DRAMCMD));
    mode = (sbport_t) PHYS_TO_K1(A_MC_REGISTER(mcnum,R_MC_DRAMMODE));


    WRITECSR(cmd,csmask | V_MC_COMMAND_CLRPWRDN);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_PRE);
    DRAMINIT_DELAY();

    /*
     * Set up for doing mode register writes to the SDRAMs
     * 
     * mode 0x62 is "sequential 4-byte bursts, CAS Latency 2.5"
     * mode 0x22 is "sequential 4-byte bursts, CAS Latency 2"
     *
     * First time, we set bit 8 to reset the DLL
     */

    WRITECSR(mode,V_MC_EMODE(SGRAM_EMRVAL) |
	     V_MC_MODE(SGRAM_MRVAL) | 
	     V_MC_MODE(SGRAM_MRVAL_RESETDLL) | 
	     V_MC_DRAM_TYPE(SGRAM));

    WRITECSR(cmd,csmask | V_MC_COMMAND_EMRS);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_MRS);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_PRE);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_AR);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_AR);
    DRAMINIT_DELAY();

    /*
     * This time, clear bit 8 to start the DLL
     */

    WRITECSR(mode,V_MC_EMODE(SGRAM_EMRVAL) | 
	     V_MC_MODE(SGRAM_MRVAL) | 
	     V_MC_DRAM_TYPE(SGRAM));
    WRITECSR(cmd,csmask | V_MC_COMMAND_MRS);
    DRAMINIT_DELAY();

}

/*  *********************************************************************
    *  SB1250_FCRAM_INITCMDS
    *  
    *  Issue the sequence of DRAM init commands.  (FCRAMs)
    *  Note: this routine does not support "big memory" (external decode)
    *  
    *  Input parameters: 
    *      mcnum - memory controller index (0/1)
    *  	   mc - pointer to data for this memory controller
    *	   csel - which chip select to send init commands for
    *      tdata - chip select to use as a template for timing data
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void sb1250_fcram_initcmds(int mcnum,mcdata_t *mc,int csel,csdata_t *tdata)
{
    uint64_t csmask;
    sbport_t cmd;
    sbport_t mode;

    csmask = M_MC_CS0 << csel;		/* convert chip select # to mask */
    cmd  = (sbport_t) PHYS_TO_K1(A_MC_REGISTER(mcnum,R_MC_DRAMCMD));
    mode = (sbport_t) PHYS_TO_K1(A_MC_REGISTER(mcnum,R_MC_DRAMMODE));


    /*
     * For FCRAMs the type must be set first, since much of the
     * init state machine is done in hardware.
     */

    WRITECSR(mode, V_MC_DRAM_TYPE(FCRAM));
    DRAMINIT_DELAY();	/* Required delay for FCRAM */

    WRITECSR(cmd,csmask | V_MC_COMMAND_CLRPWRDN);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | K_MC_COMMAND_PRE);
    DRAMINIT_DELAY();

    /*
     * Set up for doing mode register writes to the FCRAMs
     * 
     * mode 0x32 is "sequential 4-byte bursts, CAS Latency 3.0"
     */

    WRITECSR(mode,V_MC_EMODE(FCRAM_EMRVAL) |
	     V_MC_MODE(FCRAM_MRVAL) | 
	     V_MC_DRAM_TYPE(FCRAM));

    WRITECSR(cmd,csmask | V_MC_COMMAND_EMRS);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_MRS);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_AR);
    DRAMINIT_DELAY();

    WRITECSR(cmd,csmask | V_MC_COMMAND_AR);
    DRAMINIT_DELAY();

    /*
     * Do 4 dummy writes, one to each bank, to get the
     * memory started.
     */

#ifndef _MCSTANDALONE_		    /* only on real hardware */
    do {
	volatile uint64_t *ptr;

	ptr = (volatile uint64_t *) PHYS_TO_K1(0);
	*(ptr+(0x00>>3)) = 0;
	*(ptr+(0x20>>3)) = 0;
	*(ptr+(0x40>>3)) = 0;
	*(ptr+(0x60>>3)) = 0;
	} while (0);
#endif

}




/*  *********************************************************************
    *  SB1250_DRAM_INTLV
    *  
    *  Do row/column/bank initialization for 128-byte interleaved
    *  mode, and also interleave across ports.  You need to have
    *  the same geometry DIMMs installed on both memory
    *  channels for this to work.
    *
    *  Interleaved modes will assign address bits in the following
    *  order:
    *
    *  RRRRRRRR...R CCCCCCCC...C NN BB P CC xx000
    *
    *  Where 'R' are row address bits, 
    *        'C' are column address bits
    *        'N' are chip-select bits
    *        'B' are bank select bits
    *        'P' is the channel (port) select
    *        'x' is ignored by the MC, but will be set to '1'.
    *
    *  Input parameters: 
    *  	   lots of stuff
    *  	   
    *  Return value:
    *  	   lots of stuff
    ********************************************************************* */
static void sb1250_dram_intlv(initdata_t *d)
{
    int ttlbits;			/* bit counter */
    int rows,columns,banks;
    int csidx;
    sbport_t mc0base;
    sbport_t mc1base;
    sbport_t csx0base;
    sbport_t csx1base;
    uint64_t mask;
    uint64_t colmask;
    int t;
    uint64_t dimmsize;
    int num_csint = 1 << d->mc[0].csint;
    uint64_t tmp;

    d->ttlbytes = 0;			/* start with zero memory */

    /*
     * Loop through each chip select
     */

    mc0base = PHYS_TO_K1(A_MC_BASE(0));
    mc1base = PHYS_TO_K1(A_MC_BASE(1));

    for (csidx = 0; csidx < MC_CHIPSELS; csidx++) {

	/* 
	 * Address of CS-specific registers 
	 */

	csx0base = mc0base + R_MC_CSX_BASE + csidx*MC_CSX_SPACING;
	csx1base = mc1base + R_MC_CSX_BASE + csidx*MC_CSX_SPACING;

	/* 
	 * Ignore this chipsel if we're not using it 
	 */

	if (!(d->mc[0].csdata[csidx].flags & CS_PRESENT)) continue;
	if (!(d->mc[1].csdata[csidx].flags & CS_PRESENT)) continue;

	/*
	 * Remember we did something to this MC.  We won't bother
	 * activating controllers we don't use.
	 */

	d->inuse |= 3;		/* we're using both controllers 0 and 1 */

	/* 
	 * Dig the geometry out of the structure 
	 */

	columns = d->mc[0].csdata[csidx].cols;
	rows    = d->mc[0].csdata[csidx].rows;
	banks   = d->mc[0].csdata[csidx].banks;

	/*
	 * The lowest 3 bits are never set in any mask.
	 * They represent the byte width of the DIMM.
	 */

	ttlbits = 3;

	/* 
	 * The first two bits are always set and are part of the
	 * column bits.  Actually, the MC ignores these bits
	 * but we set them here for clarity.
	 *
	 * Depending on the block size, 0, 1, or 2 additional
	 * bits are used for column interleave.
	 *
	 * When interleaving ports, we always have a block
	 * size of 128.  It will work to use other block sizes,
	 * but performance will suffer.
	 */

	switch (d->mc[0].blksize) {
	    case 32:	t = 2; break;	/* 32-byte interleave */
	    case 64:	t = 3; break;	/* 64-byte interleave */
	    default:	t = 4; break;	/* 128-byte interleave */
	    }

	columns -= t;
	colmask = MAKEDRAMMASK(t,ttlbits);
	ttlbits += t;

	/*	
	 * add 1 to 'ttlbits' to account for the bit we're
	 * using for port intleave.  The current value	
	 * of 'ttlbits' also happens to be the 	
	 * bit number for port interleaving.
	 */

	d->pintbit = ttlbits;		/* interleave bit is right here */

	ttlbits++;

	/*
	 * Now do the bank mask
	 */

	mask = MAKEDRAMMASK(banks,ttlbits);
	ttlbits += banks;
	WRITECSR(csx0base+R_MC_CSX_BA,mask);
	WRITECSR(csx1base+R_MC_CSX_BA,mask);

	/*
	 * Now do the chip select mask
	 */
	
	if ((d->mc[0].csint > 0) &&
	    (csidx < num_csint)) {
	    mask = MAKEDRAMMASK(d->mc[0].csint,ttlbits);
	    ttlbits += d->mc[0].csint;
	    WRITECSR(mc0base+R_MC_CS_INTERLEAVE,mask);
	    WRITECSR(mc1base+R_MC_CS_INTERLEAVE,mask);
	    }

	/*
	 * Do the rest of the column bits
	 */

	mask = MAKEDRAMMASK(columns,ttlbits);
	colmask |= mask;
	WRITECSR(csx0base+R_MC_CSX_COL,colmask);
	WRITECSR(csx1base+R_MC_CSX_COL,colmask);
	ttlbits += columns;

	/*
	 * Finally, do the rows.  If we're in "big" memory
	 * mode, two additional row bits are used for the
	 * chip select bits.
	 */

	if (d->mc[0].flags & MCFLG_BIGMEM) {
	    rows += 2;		/* add two bits for chip select */
	    /*
	     * The "bigmem" bit will be set in the MC_CONFIG
	     * register back in the main routine.
	     */
	    }

	mask = MAKEDRAMMASK(rows,ttlbits);
	ttlbits += rows;
	WRITECSR(csx0base+R_MC_CSX_ROW,mask);
	WRITECSR(csx1base+R_MC_CSX_ROW,mask);

	/*
	 * The total size of this DIMM is 1 << ttlbits, which is inflated by a
	 * factor of num_csint to cover all interleaved chip selects.
	 */

	dimmsize = ((uint64_t) 1) << ttlbits;

	/*
	 * Program the start and end registers.  The start address is 0
	 * our if csidx is cs-interleaved; otherwise, the start address
	 * is the current "ttlbytes".
	 */

	if (csidx < num_csint) {
	    d->ttlbytes += dimmsize >> d->mc[0].csint;
	    } 
	else {
	    mask = READCSR(mc0base+R_MC_CS_START);
	    tmp = d->ttlbytes >> 24;
	    if (tmp >= 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
	    mask |= (tmp << (16*csidx));
	    WRITECSR(mc0base+R_MC_CS_START,mask);
	    
	    mask = READCSR(mc1base+R_MC_CS_START);
	    tmp = d->ttlbytes >> 24;
	    if (tmp >= 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
	    mask |= (tmp << (16*csidx));
	    WRITECSR(mc1base+R_MC_CS_START,mask);
	    
	    d->ttlbytes += dimmsize;
	    dimmsize = d->ttlbytes;  /* setup dimmsize for cs_end below */
	    }
	
	mask = READCSR(mc0base+R_MC_CS_END);
	tmp = dimmsize >> 24;
	if (tmp > 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
	mask |= (tmp << (16*csidx));
	WRITECSR(mc0base+R_MC_CS_END,mask);
	
	mask = READCSR(mc1base+R_MC_CS_END);
	tmp = dimmsize >> 24;
	if (tmp > 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
	mask |= (tmp << (16*csidx));
	WRITECSR(mc1base+R_MC_CS_END,mask);
	}
}




/*  *********************************************************************
    *  SB1250_DRAM_MSBCS
    *  
    *  Do row/column/bank initialization for MSB-CS (noninterleaved)
    *  mode.  This is only separated out of the main loop to make things
    *  read easier, it's not a generally useful subroutine by itself.
    *  
    *  Input parameters: 
    *  	   initdata_t structure
    *  	   
    *  Return value:
    *  	   memory controller initialized
    ********************************************************************* */

static void sb1250_dram_msbcs(initdata_t *d)
{
    int ttlbits;			/* bit counter */
    int rows,columns,banks;
    int mcidx,csidx;
    sbport_t mcbase;
    sbport_t csxbase;
    uint64_t mask;
    uint64_t colmask;
    int t;
    uint64_t dimmsize;

    d->ttlbytes = 0;			/* start with zero memory */

    /*
     * Loop through each memory controller and each chip select
     * within each memory controller.
     */

    for (mcidx = d->firstchan; mcidx < MC_CHANNELS; mcidx++) {
        int num_csint = 1 << d->mc[mcidx].csint;
	uint64_t channel_start = d->ttlbytes;
	uint64_t end_addr;
	uint64_t tmp;

	mcbase = PHYS_TO_K1(A_MC_BASE(mcidx));

	for (csidx = 0; csidx < MC_CHIPSELS; csidx++) {

	    /* 
	     * Address of CS-specific registers 
	     */

	    csxbase = mcbase + R_MC_CSX_BASE + csidx * MC_CSX_SPACING;

	    /* 
	     * Ignore this chipsel if we're not using it 
	     */

	    if (!(d->mc[mcidx].csdata[csidx].flags & CS_PRESENT)) continue;

	    /*
	     * Remember we did something to this MC.  We won't bother
	     * activating controllers we don't use.
	     */

	    d->inuse |= (1 << mcidx);

	    /* 
	     * Dig the geometry out of the structure 
	     */

	    columns = d->mc[mcidx].csdata[csidx].cols;
	    rows    = d->mc[mcidx].csdata[csidx].rows;
	    banks   = d->mc[mcidx].csdata[csidx].banks;

	    /*
	     * The lowest 3 bits are never set in any mask.
	     * They represent the byte width of the DIMM.
	     */

	    ttlbits = 3;

	    /* 
	     * The first two bits are always set and are part of the
	     * column bits.  Actually, the MC ignores these bits
	     * but we set them here for clarity.
	     *
	     * Depending on the block size, 0, 1, or 2 additional
	     * bits are used for column interleave.
	     */

	    switch (d->mc[mcidx].blksize) {
		case 32:   t = 2; break;	/* 32-byte interleave */
		case 64:   t = 3; break;	/* 64-byte interleave */
		default:   t = 4; break;	/* 128-byte interleave */
		}

	    columns -= t;
	    colmask = MAKEDRAMMASK(t,ttlbits);
	    ttlbits += t;

	    /*
	     * Now do the bank mask
	     */

	    mask = MAKEDRAMMASK(banks,ttlbits);
	    ttlbits += banks;
	    WRITECSR(csxbase+R_MC_CSX_BA,mask);

	    /*
	     * Now do the chip select mask
	     */
	    
	    if ((d->mc[mcidx].csint > 0) &&
		(csidx < num_csint)) {
	        mask = MAKEDRAMMASK(d->mc[mcidx].csint,ttlbits);
		ttlbits += d->mc[mcidx].csint;
		WRITECSR(mcbase+R_MC_CS_INTERLEAVE,mask);
		}

	    /*
	     * Do the rest of the column bits
	     */

	    mask = MAKEDRAMMASK(columns,ttlbits);
	    colmask |= mask;
	    WRITECSR(csxbase+R_MC_CSX_COL,colmask);
	    ttlbits += columns;

	    /*
	     * Finally, do the rows.  If we're in "big" memory
	     * mode, two additional row bits are used for the
	     * chip select bits.
	     */

	    if (d->mc[mcidx].flags & MCFLG_BIGMEM) {
		rows += 2;		/* add two bits for chip select */
		/*
		 * The "bigmem" bit will be set in the MC_CONFIG
		 * register back in the main routine.
 	 	 */
		}

	    mask = MAKEDRAMMASK(rows,ttlbits);
	    ttlbits += rows;
	    WRITECSR(csxbase+R_MC_CSX_ROW,mask);

	    /*
	     * The total size of this DIMM is 1 << ttlbits, which is inflated
	     * by a factor of num_csint to cover all interleaved chip selects.
	     */

	    dimmsize = ((uint64_t) 1) << ttlbits;

	    /*
	     * Program the start and end registers.  The start address is
	     * channel_start if csidx is cs-interleaved; otherwise, the
	     * start address is the current "ttlbytes".
	     */

	    if (csidx < num_csint) {
	        mask = READCSR(mcbase+R_MC_CS_START);
		tmp  = (channel_start >> 24);
		if (tmp >= 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
		mask |= (tmp << (16*csidx));
		WRITECSR(mcbase+R_MC_CS_START,mask);

	        d->ttlbytes += dimmsize >> d->mc[mcidx].csint;
		end_addr = channel_start + dimmsize;
		} 
	    else {
	        mask = READCSR(mcbase+R_MC_CS_START);
		tmp  = d->ttlbytes >> 24;
		if (tmp >= 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
		mask |= (tmp << (16*csidx));
		WRITECSR(mcbase+R_MC_CS_START,mask);
	    
		d->ttlbytes += dimmsize;
		end_addr = d->ttlbytes;
		}
	
	    mask = READCSR(mcbase+R_MC_CS_END);
	    tmp  = end_addr >> 24;
	    if (tmp > 0x40) tmp = (0x100 + (tmp - 0x40)); 	/* Adj for exp space */
	    mask |= (tmp << (16*csidx));
	    WRITECSR(mcbase+R_MC_CS_END,mask);
	    }
    }
}


/*  *********************************************************************
    *  SB1250_DRAM_ANALYZE(d)
    *  
    *  Analyze the DRAM parameters, determine if we can do 
    *  port interleaving mode
    *  
    *  Input parameters: 
    *  	   d - init data
    *  	   
    *  Return value:
    *  	   nothing (fields in initdata are updated)
    ********************************************************************* */

static void sb1250_dram_analyze(initdata_t *d)
{
    int csidx;
    int mcidx;
    int csint;

    /*
     * Determine if we can do port interleaving.  This is possible if
     * the DIMMs on each channel are the same.
     */

    for (csidx = 0; csidx < MC_CHIPSELS; csidx++) {
	if (d->mc[0].csdata[csidx].rows != d->mc[1].csdata[csidx].rows) break;
	if (d->mc[0].csdata[csidx].cols != d->mc[1].csdata[csidx].cols) break;
	if (d->mc[0].csdata[csidx].banks != d->mc[1].csdata[csidx].banks) break;
	if (d->mc[0].csdata[csidx].flags != d->mc[1].csdata[csidx].flags) break;
	}

    /*
     * If the per-controller flags don't match, no port interleaving.
     * I.e., you can't mix and match ECC, big memory, etc.
     */

    if (d->mc[0].flags != d->mc[1].flags) csidx = 0;

    /*
     * Done with checks, see if we can do it.
     */

    if (csidx == MC_CHIPSELS) {
	/* 
	 * All channels are the same, we can interleave. If we were asked
	 * to try it, then enable it.
	 */
	if (d->flags & M_MCINIT_TRYPINTLV) d->flags |= M_MCINIT_PINTLV;
	}

    /*
     * Determine how many CS interleave bits (0, 1, or 2) will work.
     * Memory channels are checked separately.  If port (i.e., channel)
     * interleaving is allowed, each channel will end up with the same number
     * of CS interleave bits.
     * Note: No support for interleaving only chip selects 2 & 3.
     */

    for (mcidx = d->firstchan; mcidx < MC_CHANNELS; mcidx++) {
        /* Forbid CS interleaving if any of:
	   - not requested
	   - in large mem mode
	   - CS 0 is absent */
	if ((d->mc[mcidx].cfgcsint > 0) &&
	    !(d->mc[mcidx].flags & MCFLG_BIGMEM) &&
	    (d->mc[mcidx].csdata[0].flags & CS_PRESENT)) {

	    for (csidx = 1; csidx < MC_CHIPSELS; csidx++) {
		/* CS csidx must be present */
		if (!(d->mc[mcidx].csdata[csidx].flags & CS_PRESENT)) break;
		/* CS csidx must match geometry of CS 0 */
		if (d->mc[mcidx].csdata[0].rows != d->mc[mcidx].csdata[csidx].rows)
		    break;
		if (d->mc[mcidx].csdata[0].cols != d->mc[mcidx].csdata[csidx].cols)
		    break;
		if (d->mc[mcidx].csdata[0].banks !=
		    d->mc[mcidx].csdata[csidx].banks)
		    break;
		}	    
	    /* csidx = 1st CS index that can't be interleaved;
	       Convert csidx to a number of CS interleave address bits */
	    csint = csidx >> 1;
	    /* Cap csint by the csinterleave attribute on the channel */
	    if (csint > d->mc[mcidx].cfgcsint) {
		csint = d->mc[mcidx].cfgcsint;
		}
	    /* Forbid CS interleaving into the hole in the memory address
	       space; i,e., cap the port-and-CS-interleaved CS size at 1 GB.
	       Remove this code when sb1250_dram_intlv() can deal with CS
	       sizes that span the hole. */
	    {
	      int addr_bits = 30;  /* 1 GB */
	      addr_bits -= 3;      /* 8 byte data bus width */
	      addr_bits -= d->mc[mcidx].csdata[0].rows;
	      addr_bits -= d->mc[mcidx].csdata[0].cols;
	      addr_bits -= d->mc[mcidx].csdata[0].banks;
	      if (d->flags & M_MCINIT_PINTLV) addr_bits -= 1;
	      if (addr_bits < 0) addr_bits = 0;
	      if (addr_bits < csint) csint = addr_bits;
	    }
	    /* Return csint to caller */
	    d->mc[mcidx].csint = csint;
	    }
	}
}




#if !defined(_MCSTANDALONE_)		    /* When not linked into firmware, no RAM zeroing */

/*  *********************************************************************
    *  SB1250_DRAM_ZERO1MB(d,addr)
    *  
    *  Zero one megabyte of memory starting at the specified address.
    *  'addr' is in megabytes.
    *  
    *  Input parameters: 
    *  	   d - initdata structure
    *  	   addr - starting address, expressed as a megabyte index
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifndef _DMZERO_
static void sb1250_dram_zero1mb(initdata_t *d,uint64_t addr)
{
    __asm(" .set push ; .set noreorder ; .set mips64 ; "
	  "  mfc0 $9,$12 ; "
	  "  ori  $8,$9,0x80 ; "
	  "  mtc0 $8,$12 ; "
	  "  bnel $0,$0,.+4 ; "
	  "  ssnop ; "
	  "  lui  $10,0xB800 ; "
	  "  dsll32 $10,$10,0 ; "
	  "  dsll   $12,%0,20 ;"
	  "  or     $10,$10,$12 ; "
	  "  lui    $11,0x10 ; "
	  "ClrLoop: "
	  "  sd     $0,0($10) ; " 
	  "  sd     $0,8($10) ; " 
	  "  sd     $0,16($10) ; " 
	  "  sd     $0,24($10) ; " 
	  "  sd     $0,32($10) ; " 
	  "  sd     $0,40($10) ; " 
	  "  sd     $0,48($10) ; " 
	  "  sd     $0,56($10) ; "
	  "  sub    $11,$11,64 ; "
	  "  bne    $11,$0,ClrLoop ;  "
	  "  dadd   $10,64 ; "
          "  mtc0   $9,$12 ; "
          "  bnel   $0,$0,.+4 ;"
	  "  ssnop ; "
	  " .set pop"
	  : : "r"(addr) : "$8","$9","$10","$11","$12");
}


/*  *********************************************************************
    *  SB1250_DRAM_ZERO1MB(d,addr)
    *  
    *  Zero one megabyte of memory starting at the specified address.
    *  'addr' is in megabytes.
    *  
    *  Input parameters: 
    *  	   d - initdata structure
    *  	   addr - starting address, expressed as a megabyte index
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifdef _DMZERO_
static void sb1250_dram_zero1mb(initdata_t *d,uint64_t addr)
{
    sbport_t dmreg;
    uint64_t baseaddr;
    volatile int idx;

    /*
     * Build the descriptor
     */
    
    d->dscr[0] = (addr << 20) |
	M_DM_DSCRA_ZERO_MEM | 
	M_DM_DSCRA_UN_DEST | M_DM_DSCRA_UN_SRC |
	V_DM_DSCRA_DIR_SRC_CONST |
	V_DM_DSCRA_DIR_DEST_INCR;
    d->dscr[1] = V_DM_DSCRB_SRC_LENGTH(0);

    /* Flush the descriptor out.  We need to do this in Pass1
       because we're in cacheable noncoherent mode right now and
       the core will not respond to the DM's request for the descriptor. */

    __asm __volatile ("cache 0x15,0(%0) ; " :: "r"(d));

    /*
     * Give the descriptor to the data mover 
     */

    dmreg = PHYS_TO_K1(A_DM_REGISTER(0,R_DM_DSCR_BASE));
    baseaddr = (uint64_t) K0_TO_PHYS((long)d->dscr) | 
	V_DM_DSCR_BASE_PRIORITY(0) |
	V_DM_DSCR_BASE_RINGSZ(4) |
	M_DM_DSCR_BASE_ENABL |
	M_DM_DSCR_BASE_RESET;
    WRITECSR(dmreg,baseaddr);

    dmreg = PHYS_TO_K1(A_DM_REGISTER(0,R_DM_DSCR_COUNT));
    WRITECSR(dmreg,1);

    /*
     * Wait for the request to complete
     */

    while ((READCSR(dmreg) & 0xFFFF) > 0) {
	/* Do something that doesn't involve the ZBBus to give
	   the DM some extra time */
	for (idx = 0; idx < 10000; idx++) ; /* NULL LOOP */
	}

}
#endif


/*  *********************************************************************
    *  SB1250_DRAM_ZERO(d)
    *  
    *  Zero memory, using the data mover.
    *  
    *  Input parameters: 
    *  	   d - initdata structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void sb1250_dram_zero(initdata_t *d)
{
    int idx;
    int maxmem;
    uint64_t curmb;			/* current address in megabytes */

    maxmem = (int) (d->ttlbytes >> 20);
    curmb = 0;

    for (idx = 0; idx < (int) maxmem; idx++) {
	sb1250_dram_zero1mb(d,curmb);
	curmb++;
	if (curmb == (REGION0_LOC+REGION0_SIZE))      curmb = REGION1_LOC;
	else if (curmb == (REGION1_LOC+REGION1_SIZE)) curmb = REGION2_LOC;
	else if (curmb == (REGION2_LOC+REGION2_SIZE)) curmb = REGION3_LOC;
	}
}

#endif

#endif

/*  *********************************************************************
    *  SB1250_DRAM_INIT()
    *  
    *  Initialize DRAM connected to the specified DRAM controller
    *  The DRAM will be configured without interleaving, as sequential
    *  blocks of memory.
    *  
    *  Input parameters: 
    *  	   a0 - zero to use default memconfig table
    *           or KSEG1 address of mem config table
    *  	   
    *  Return value:
    *  	   v0 - total amount of installed DRAM
    *  
    *  Registers used:
    *  	   all
    ********************************************************************* */

uint64_t sb1250_dram_init_real(const draminittab_t *init,initdata_t *d);
uint64_t sb1250_dram_init_real(const draminittab_t *init,initdata_t *d)
{
    uint64_t reg;
    sbport_t mcbase;
    uint64_t cfgbits;
    int mcidx;
    int csidx;
    int dramtype;
    csdata_t *tdata;
    mcdata_t *mc;
    uint32_t *d32;
    int idx;
    const draminittab_t *init_11xx;
    const draminittab_t *init_12xx;

#if !defined(_MCSTANDALONE_)
#if CFG_EMBEDDED_PIC
    __asm(".set push ; .set noreorder ; "
	  " bal LoadREL1 ; "
	  " nop ; "
	  "LoadREL1: la %0,draminittab_11xx-LoadREL1 ; "
	  " add %0,$31 ; "
	  " .set pop "
	  : "=r"(init_11xx) :: "$31");

    __asm(".set push ; .set noreorder ; "
	  " bal LoadREL2 ; "
	  " nop ; "
	  "LoadREL2: la %0,draminittab_12xx-LoadREL2 ; "
	  " add %0,$31 ; "
	  " .set pop "
	  : "=r"(init_12xx) :: "$31");
#else
    init_11xx = (draminittab_t *) draminittab_11xx;
    init_12xx = (draminittab_t *) draminittab_12xx;
#endif
#endif

    /*
     * Zero the entire initdata structure
     */

    d32 = (uint32_t *) d;
    for (idx = 0; idx < sizeof(initdata_t)/sizeof(uint32_t); idx++) {
	*d32++ = 0;
	}

    /*
     * Determine system SOC type so we will know what channels
     * to initialize.  The hybrid parts have 1250s inside,
     * so even though there are only pins for 1 channel the 
     * registers are there for both, so we need to initialize them.
     * The "real" 1125s only have one channel.
     */

    cfgbits = READCSR(PHYS_TO_K1(A_SCD_SYSTEM_REVISION));
    d->soctype = SYS_SOC_TYPE(cfgbits);

    switch (d->soctype) {
	case  K_SYS_SOC_TYPE_BCM1120:
	case  K_SYS_SOC_TYPE_BCM1125:
	    d->firstchan = 1;
#if !defined(_MCSTANDALONE_)
	    if (!init) init = init_11xx;
#endif
	    break;
	  
	case K_SYS_SOC_TYPE_BCM1250:
	case K_SYS_SOC_TYPE_BCM1125H:
	default:
	    d->firstchan = 0;
#if !defined(_MCSTANDALONE_)
	    if (!init) init = init_12xx;
#endif
	}

    /*
     * Begin by initializing the memory channels to some known state.  
     * Set the "BERR_DISABLE" bit for now while we initialize the channels,
     * this will be cleared again before the routine exits.
     */

#ifdef _MCSTANDALONE_NOISY_
    printf("DRAM: Initializing memory controller.\n");
#endif

    for (mcidx = d->firstchan; mcidx < MC_CHANNELS; mcidx++) {
	mcbase = PHYS_TO_K1(A_MC_BASE(mcidx));

	WRITECSR(mcbase+R_MC_CONFIG,V_MC_CONFIG_DEFAULT | M_MC_ECC_DISABLE | 
		 V_MC_CS_MODE_MSB_CS | M_MC_BERR_DISABLE);
	WRITECSR(mcbase+R_MC_CS_START,0);
	WRITECSR(mcbase+R_MC_CS_END,0);
	WRITECSR(mcbase+R_MC_CS_INTERLEAVE,0);
	WRITECSR(mcbase+R_MC_CS_ATTR,0);
	WRITECSR(mcbase+R_MC_TEST_DATA,0);
	WRITECSR(mcbase+R_MC_TEST_ECC,0);
	}

    /*
     * Read the parameters
     */

    sb1250_dram_readparams(d,init);

    /*
     * Analyze parameters 
     */

    sb1250_dram_analyze(d);

    /*
     * Configure chip selects
     */

    if (d->flags & M_MCINIT_PINTLV) {
	sb1250_dram_intlv(d);
	cfgbits = V_MC_CHANNEL_SEL(d->pintbit);
	}
    else {
	sb1250_dram_msbcs(d);
	cfgbits = 0;
	}

    /*
     * Okay, initialize the DRAM controller(s)
     */

    for (mcidx = d->firstchan; mcidx < MC_CHANNELS; mcidx++) {

        uint64_t mc_cfgbits = cfgbits;

	/*
	 * Skip this controller if we did nothing
	 */
	if (!(d->inuse & (1 << mcidx))) continue;

	/*
	 * Get the base address of the controller
	 */
	mcbase = PHYS_TO_K1(A_MC_BASE(mcidx));

	/* 
	 * Get our MC data 
	 */

	mc = &(d->mc[mcidx]);

	/*
	 * Program the clock config register.  This starts the clock to the
	 * SDRAMs.  Need to wait 200us after doing this. (6.4.6.1)
	 *
	 * Find the slowest chip/dimm among the chip selects on this 
	 * controller and use that for computing the timing values.
	 */ 

	csidx = sb1250_find_timingcs(mc);
	if (csidx < 0) continue;		/* should not happen */

	tdata = &(d->mc[mcidx].csdata[csidx]);	/* remember for use below */

	if (mc->mantiming) {
	    sb1250_manual_timing(mcidx,mc);
	    }
	else {
	    sb1250_auto_timing(mcidx,mc,tdata);
	    }

	DRAMINIT_DELAY();

	/*
	 * Set up the memory controller config and timing registers.
	 */

	switch(mc->csint) {
	    case 0: mc_cfgbits |= V_MC_CS_MODE_MSB_CS;      break;
	    case 1: mc_cfgbits |= V_MC_CS_MODE_MIXED_CS_32; break;
	    case 2: mc_cfgbits |= V_MC_CS_MODE_INTLV_CS;    break;
	    }

	mc_cfgbits |= V_MC_WR_LIMIT_DEFAULT | V_MC_AGE_LIMIT_DEFAULT | 
	    V_MC_BANK0_MAP_DEFAULT | V_MC_BANK1_MAP_DEFAULT | 
	    V_MC_BANK2_MAP_DEFAULT | V_MC_BANK3_MAP_DEFAULT | 
	    V_MC_QUEUE_SIZE_DEFAULT;

	/* Give IOB1 priority (config bit is only on channel 1) */

	if (mcidx == 1) mc_cfgbits |= M_MC_IOB1HIGHPRIORITY;

	WRITECSR(mcbase+R_MC_CONFIG,mc_cfgbits | M_MC_ECC_DISABLE | M_MC_BERR_DISABLE);


	dramtype = d->mc[mcidx].dramtype;
	
	/*
	 * Set the page policy
	 */

	WRITECSR(mcbase+R_MC_CS_ATTR,
		 V_MC_CS0_PAGE(mc->pagepolicy) | 
		 V_MC_CS1_PAGE(mc->pagepolicy) | 
		 V_MC_CS2_PAGE(mc->pagepolicy) | 
		 V_MC_CS3_PAGE(mc->pagepolicy));

	/*
	 * Okay, now do the following sequence:
	 * PRE-EMRS-MRS-PRE-AR-AR-MRS.  Do this for each chip select,
	 * one at a time for each enabled chip select.
	 */

	for (csidx = 0; csidx < MC_CHIPSELS; csidx++) {
	    if (mc->csdata[csidx].flags & CS_PRESENT) {

		switch (dramtype) {
		    case JEDEC:
			if (mc->flags & MCFLG_BIGMEM) {
			    sb1250_jedec_initcmds(mcidx,mc,csidx,0,tdata);
			    sb1250_jedec_initcmds(mcidx,mc,csidx,1,tdata);
			    sb1250_jedec_initcmds(mcidx,mc,csidx,2,tdata);
			    sb1250_jedec_initcmds(mcidx,mc,csidx,3,tdata);
			    /*
			     * If in "big memory mode" turn on the "external decode"
			     * switch here.  We never turn it off.
			     */

			    if (mc->flags & MCFLG_BIGMEM) {
				sbport_t port;
				port = PHYS_TO_K1(A_MC_REGISTER(mcidx,R_MC_DRAMMODE));
				WRITECSR(port,M_MC_EXTERNALDECODE);
				}
			    }
			else {
			    sb1250_jedec_initcmds(mcidx,mc,csidx,0,tdata);
			    }
			break;
		    case SGRAM:
			sb1250_sgram_initcmds(mcidx,mc,csidx,tdata);
			break;
		    case FCRAM:
			sb1250_fcram_initcmds(mcidx,mc,csidx,tdata);
			break;
		    default:
#ifdef _MCSTANDALONE_NOISY_
			printf("DRAM: Channel DRAM type declared as DRAM_TYPE_SPD, but no SPD DRAM type found.\n");
#endif
			break;
		    }

		}
	    }

	/*
	 * Kill the BERR_DISABLE bit for this controller
	 */

	reg = READCSR(mcbase+R_MC_CONFIG);
	reg &= ~M_MC_BERR_DISABLE;
	WRITECSR(mcbase+R_MC_CONFIG,reg);
	}

#if !defined(_MCSTANDALONE_)
    /*
     * Zero the contents of memory to set the ECC bits correctly.
     * Do it for all memory if either channel is enabled for ECC.
     */

    for (mcidx = d->firstchan; mcidx < MC_CHANNELS; mcidx++) {
	if (!(d->inuse & (1 << mcidx))) continue;
	if (d->mc[mcidx].flags & MCFLG_ECC_ENABLE) {
	    sb1250_dram_zero(d);
	    break;
	    }
	}
#endif

    /*
     * Turn on the ECC in the memory controller for those channels
     * that we've specified.
     */

    for (mcidx = d->firstchan; mcidx < MC_CHANNELS; mcidx++) {
	if (!(d->inuse & (1 << mcidx))) continue;
	if (!(d->mc[mcidx].flags & MCFLG_ECC_ENABLE)) continue;		/* ecc not enabled */
	mcbase = PHYS_TO_K1(A_MC_BASE(mcidx));
	reg = READCSR(mcbase+R_MC_CONFIG);
	reg &= ~M_MC_ECC_DISABLE;
	WRITECSR(mcbase+R_MC_CONFIG,reg);
	}

    /*
     * Return the total amount of memory initialized, in megabytes
     */

#ifdef _MCSTANDALONE_NOISY_
    printf("DRAM: Total memory: %dMB.\n",(unsigned int)(d->ttlbytes >> 20));
#endif

    return (d->ttlbytes >> 20);
}


/*  *********************************************************************
    *  XXSB1250_DRAMINIT()
    *  
    *  This is a hideous hack.  To help keep things all in one module,
    *  and to aid in relocation (remember, it's tough to do a 
    *  PC-relative branch to an external symbol), here is an 
    *  assembly stub to get things ready to call the above C routine.
    *  We turn off the bus errors on both memory controllers, set up
    *  a small stack, and branch to the C routine to handle the rest.
    *  
    *  Input parameters: 
    *  	   register a0 - user initialization table
    *  	   
    *  Return value:
    *  	   register v0 - size of memory, in bytes
    ********************************************************************* */

#if !defined(_MCSTANDALONE_)
static void __attribute__ ((unused)) xxsb1250_draminit(const draminittab_t *init)
{
    __asm(" .globl sb1250_dram_init ; "
	  "sb1250_dram_init: ; "
	  " dli $10,0x30158A00C9800000 ; "	/* Set the BERR_DISABLE bits */
	  " lui	$8,0xb005 ; "			/* and ECC_DISABLE bits */
	  " sd	$10,0x1100($8) ; "		/* do MC 0 */
	  " sd	$10,0x2100($8) ; "		/* do MC 1 */
#ifdef _VERILOG_
	  " li $29,0x9FFFFFF0 ; "		/* Set up a stack pointer */
	  " li $5,0x9FFFFC00 ; "		/* Pass pointer to data area */
#else
	  " li $29,0x80000400 ; "		/* Set up a stack pointer */
	  " li $5,0x80000000 ; "		/* Pass pointer to data area */
#endif
	  " b sb1250_dram_init_real ; ");	/* Branch to real init routine */

}

#else	/* _MCSTANDALONE_ */

/* 
 * SOCVIEW and non-CFE, non-MIPS things don't need any magic since they
 * are not running on the 1250.  Just call the main routine.
 */
uint64_t sb1250_dram_init(const draminittab_t *init,initdata_t *d)
{
    initdata_t initdata;
    return sb1250_dram_init_real(init,&initdata);
}
#endif


/*  *********************************************************************
    *  End  (yes, 3000 lines of memory controller init code.  Sheesh!)
    ********************************************************************* */
