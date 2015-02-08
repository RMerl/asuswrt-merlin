/*
#************************************************************************
#* Coprocessor 0 Register Names
#************************************************************************
*/
#define BCM6345_INDEX           $0
#define BCM6345_ENTRY           $2
#define BCM6345_BVADDR          $8
#define BCM6345_COUNT           $9
#define BCM6345_COMPARE         $11
#define BCM6345_STATUS          $12
#define BCM6345_CAUSE           $13
#define BCM6345_EPC             $14
#define BCM6345_PRID            $15
#define BCM6345_PROCCFG         $16
#define BCM6345_CONFIG          $22
#define BCM6345_DEBUG           $23
#define BCM6345_DBEXCPC         $24
#define BCM6345_DESAVE          $31

#define BRCM_CONFIG             $22

/*
#************************************************************************
#* Coprocessor 0 Index Register Bits
#************************************************************************
# No known functionality (32-bit register)
*/

/*
#************************************************************************
#* Coprocessor 0 Entry Register Bits
#************************************************************************
# No known functionality (32-bit register)
*/

/*
#************************************************************************
#* Coprocessor 0 BvAddr Register Bits
#************************************************************************
# Contains offending address (32-bit register)
*/

/*
#************************************************************************
#* Coprocessor 0 Status Register Bits
#************************************************************************

# Notes: COP2 is forced, then allowed to be overwritten
#               writing a '1' to bit 20 force bit 20 to '0' but no effect
*/
#define CP0SR_COP3              (1<<31)
#define CP0SR_COP2              (1<<30)
#define CP0SR_COP1              (1<<29)
#define CP0SR_COP0              (1<<28)
#define CP0SR_IST               (1<<23)
#define CP0SR_BEV               (1<<22)
#define CP0SR_SWC               (1<<17)
#define CP0SR_ISC               (1<<16)
#define CP0SR_KRNL              (1<<1)
#define CP0SR_IE                (1<<0)
#define CP0SR_IM5               (1<<15)
#define CP0SR_IM4               (1<<14)
#define CP0SR_IM3               (1<<13)
#define CP0SR_IM2               (1<<12)
#define CP0SR_IM1               (1<<11)
#define CP0SR_IM0               (1<<10)
#define CP0SR_SWM1              (1<<9)
#define CP0SR_SWM0              (1<<8)

/*
#************************************************************************
#* Coprocessor 0 Cause Register Bits
#************************************************************************
# Notes: 5:2 hold exception cause
# Notes: 29:28 hold Co-processor Number reference by Coproc unusable excptn
# Notes: 7:6, 1:0, 27:15, 30 ***UNUSED***
*/
#define CP0CR_BD                        (1<<31)
#define CP0CR_EXTIRQ4                   (1<<14)
#define CP0CR_EXTIRQ3                   (1<<13)
#define CP0CR_EXTIRQ2                   (1<<12)
#define CP0CR_EXTIRQ1                   (1<<11)
#define CP0CR_EXTIRQ0                   (1<<10)
#define CP0CR_SW1                       (1<<9)
#define CP0CR_SW0                       (1<<8)
#define CP0CR_EXC_CAUSE_MASK            (0xf << 2)
#define CP0CR_EXC_COP_MASK              (0x3 << 28)

/*
#************************************************************************
#* Coprocessor 0 EPC Register Bits
#************************************************************************
# Contains PC or PC-4 for resuming program after exception (32-bit register)
*/

/*
#************************************************************************
#* Coprocessor 0 PrID Register Bits
#************************************************************************
# Notes: Company Options=0
#        Company ID=0
#        Processor ID = 0xa
#        Revision = 0xa
*/

/*
#************************************************************************
#* Coprocessor 0 Debug Register Bits
#************************************************************************
# Notes: Bits [29:13],[11], [9], [6] read as zero
*/
#define CP0_DBG_BRDLY           (0x1 << 31)
#define CP0_DBG_DBGMD           (0x1 << 30)
#define CP0_DBG_EXSTAT          (0x1 << 12)
#define CP0_DBG_BUS_ERR         (0x1 << 10)
#define CP0_DBG_1STEP           (0x1 << 8)
#define CP0_DBG_JTGRST          (0x1 << 7)
#define CP0_DBG_PBUSBRK         (0x1 << 5)
#define CP0_DBG_IADBRK          (0x1 << 4)
#define CP0_DBG_DABRKST         (0x1 << 3)
#define CP0_DBG_DABRKLD         (0x1 << 2)
#define CP0_DBG_SDBBPEX         (0x1 << 1)
#define CP0_DBG_SSEX            (0x1 << 0)

/*
#************************************************************************
#* Coprocessor 0 DBEXCPC Register Bits
#************************************************************************
# Debug Exception Program Counter (32-bits)
*/

/*
#************************************************************************
#* Coprocessor 0 PROCCFG Register Bits
#************************************************************************
# Select 0
*/
#define CP0_CFG1EN              (0x1 << 31) 
#define CP0_BE                  (0x1 << 15)
#define CP0_MIPS32MSK           (0x3 << 13) /* 0 = MIPS32 Arch */
#define CP0_ARMSK               (0x7 << 10) /* Architecture Rev 0 */
#define CP0_MMUMSK              (0x7 << 7)  /* 0 no MMU */

#define CP0_K0Coherency         (0x7 << 0)  /* 0 no Coherency */
#define CP0_K0WriteThrough      (0x1 << 0)  /* 1 = Cached, Dcache write thru */
#define CP0_K0Uncached          (0x2 << 0)  /* 2 = Uncached */
#define CP0_K0Writeback         (0x3 << 0)  /* 3 = Cached, Dcache write back */

/*
# Select 1
#  Bit  31:   unused
#  Bits 30:25 MMU Size (Num TLB entries-1)
#  Bits 24:22 ICache sets/way (2^n * 64)
#  Bits 21:19 ICache Line size (2^(n+1) bytes) 0=No Icache
#  Bits 18:16 ICache Associativity (n+1) way                    
#  Bits 15:13 DCache sets/way (2^n * 64)
#  Bits 12:10 DCache Line size (2^(n+1) bytes) 0=No Dcache
#  Bits 9:7   DCache Associativity (n+1) way                    
#  Bits 6:4   unused
#  Bit  3:    1=At least 1 watch register
#  Bit  2:    1=MIPS16 code compression implemented
#  Bit  1:    1=EJTAG implemented                   
#  Bit  0:    1=FPU implemented                   
*/
#define CP0_CFG_ISMSK      (0x7 << 22)
#define CP0_CFG_ISSHF      22
#define CP0_CFG_ILMSK      (0x7 << 19)
#define CP0_CFG_ILSHF      19
#define CP0_CFG_IAMSK      (0x7 << 16)
#define CP0_CFG_IASHF      16
#define CP0_CFG_DSMSK      (0x7 << 13)
#define CP0_CFG_DSSHF      13
#define CP0_CFG_DLMSK      (0x7 << 10)
#define CP0_CFG_DLSHF      10
#define CP0_CFG_DAMSK      (0x7 << 7)
#define CP0_CFG_DASHF      7

/*
#************************************************************************
#* Coprocessor 0 Config Register Bits
#************************************************************************
*/
#define CP0_CFG_ICSHEN         (0x1 << 31)
#define CP0_CFG_DCSHEN         (0x1 << 30)
#define CP0_CFG_FMEN           (0x1 << 29)

/*
#************************************************************************
#* Coprocessor 0 DeSave Register Bits
#************************************************************************
# Note: No apparent functionality 32-b R/W
*/
