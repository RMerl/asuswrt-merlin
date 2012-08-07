/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  MIPS64 CPU definitions			File: sbmips.h
    * 
    *  This module contains constants and macros specific to the
    *  SB1 MIPS64 core.
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

#ifndef _SB_MIPS_H
#define _SB_MIPS_H

/*  *********************************************************************
    *  Configure language
    ********************************************************************* */

#if defined(__ASSEMBLER__)
#define _ATYPE_
#define _ATYPE32_
#define _ATYPE64_
#else
#define _ATYPE_		(__SIZE_TYPE__)
#define _ATYPE32_	(int)
#define _ATYPE64_	(long long)
#endif


/*  *********************************************************************
    *  Bitfield macros
    ********************************************************************* */

/*
 * Make a mask for 1 bit at position 'n'
 */

#define _MM_MAKEMASK1(n) (1 << (n))

/*
 * Make a mask for 'v' bits at position 'n'
 */

#define _MM_MAKEMASK(v,n) (((1<<(v))-1) << (n))

/*
 * Make a value at 'v' at bit position 'n'
 */

#define _MM_MAKEVALUE(v,n) ((v) << (n))

/*
 * Retrieve a value from 'v' at bit position 'n' with 'm' mask bits
 */

#define _MM_GETVALUE(v,n,m) (((v) & (m)) >> (n))



/*  *********************************************************************
    *  32-bit MIPS Address Spaces
    ********************************************************************* */

#ifdef __ASSEMBLER__

#ifdef __mips64		    /* If 64-bit GPRs, need to sign extend.  */
#define _ACAST32_	0xffffffff00000000 |
#else
#define _ACAST32_
#endif /* __mips64  */
#define _ACAST64_

#else

#define _ACAST32_	_ATYPE_ _ATYPE32_	/* widen if necessary */
#define _ACAST64_		_ATYPE64_	/* do _not_ narrow */

#endif

/* 32-bit address map */
#define UBASE		0x00000000		/* user+ mapped */
#define USIZE		0x80000000
#define K0BASE 		(_ACAST32_ 0x80000000)	/* kernel unmapped cached */
#define K0SIZE 		0x20000000
#define K1BASE 		(_ACAST32_ 0xa0000000)	/* kernel unmapped uncached */
#define K1SIZE 		0x20000000
#define KSBASE 		(_ACAST32_ 0xc0000000)	/* supervisor+ mapped */
#define KSSIZE 		0x20000000
#define K3BASE 		(_ACAST32_ 0xe0000000)	/* kernel mapped */
#define K3SIZE 		0x20000000

/* 64-bit address map additions to the above (sign-extended) ranges */
#define XUBASE		(_ACAST64_ 0x0000000080000000)	/* user+ mapped */
#define XUSIZE		(_ACAST64_ 0x00000FFF80000000)
#define XSSEGBASE       (_ACAST64_ 0x4000000000000000)	/* supervisor+ mapped */
#define XSSEGSIZE	(_ACAST64_ 0x0000100000000000)
#define XKPHYSBASE      (_ACAST64_ 0x8000000000000000)	/* kernel unmapped */
#define XKPHYSSIZE	(_ACAST64_ 0x0000100000000000)
#define XKSEGBASE       (_ACAST64_ 0xC000000000000000)	/* kernel mapped */
#define XKSEGSIZE	(_ACAST64_ 0x00000FFF80000000)

#define GEN_VECT 	(_ACAST32_ 0x80000080)
#define UTLB_VECT 	(_ACAST32_ 0x80000000)

/*  *********************************************************************
    *  Address space coercion macros
    ********************************************************************* */

#define PHYS_TO_K0(pa)	(K0BASE | (pa))
#define PHYS_TO_K1(pa)	(K1BASE | (pa))
#define K0_TO_PHYS(va)	((va) & (K0SIZE-1))
#define K1_TO_PHYS(va)	((va) & (K1SIZE-1))
#define K0_TO_K1(va)	((va) | K1SIZE)
#define K1_TO_K0(va)	((va) & ~K1SIZE)

#define PHYS_TO_XK1(p) (_ACAST64_ (0xffffffffa0000000 | (p)))
#define XK1_TO_PHYS(p) ((p) & (K1SIZE-1))
#define PHYS_TO_XKPHYS(cca,p) (_SB_MAKEMASK1(63) | (_SB_MAKE64(cca) << 59) | (p))
#define PHYS_TO_XKSEG_UNCACHED(p)  PHYS_TO_XKPHYS(K_CALG_UNCACHED,(p))
#define PHYS_TO_XKSEG_CACHED(p)    PHYS_TO_XKPHYS(K_CALG_COH_SHAREABLE,(p))
#define XKPHYS_TO_PHYS(p) ((p) & _SB_MAKEMASK(0,59))


#if !defined(__ASSEMBLER__)
#define mips_wbflush()  __asm__ __volatile__ ("sync" : : : "memory")
#define	ISK0SEG(va)	((va) >= K0BASE && (va) <= (K0BASE + K0SIZE - 1))
#define	ISK1SEG(va)	((va) >= K1BASE && (va) <= (K1BASE + K1SIZE - 1))
#endif

/*  *********************************************************************
    *  Register aliases
    ********************************************************************* */

#if defined(__ASSEMBLER__)
#define zero		$0
#define	AT		$1		/* assembler temporaries */
#define	v0		$2		/* value holders */
#define	v1		$3
#define	a0		$4		/* arguments */
#define	a1		$5
#define	a2		$6
#define	a3		$7
#define	t0		$8		/* temporaries */
#define	t1		$9
#define	t2		$10
#define	t3		$11
#define	t4		$12
#define	t5		$13
#define	t6		$14
#define	t7		$15
#define ta0		$12
#define ta1		$13
#define ta2		$14
#define ta3		$15
#define	s0		$16		/* saved registers */
#define	s1		$17
#define	s2		$18
#define	s3		$19
#define	s4		$20
#define	s5		$21
#define	s6		$22
#define	s7		$23
#define	t8		$24		/* temporaries */
#define	t9		$25
#define	k0		$26		/* kernel registers */
#define	k1		$27
#define	gp		$28		/* global pointer */
#define	sp		$29		/* stack pointer */
#define	s8		$30		/* saved register */
#define	fp		$30		/* frame pointer */
#define	ra		$31		/* return address */
#endif

/*  *********************************************************************
    *  CP0 Registers 
    ********************************************************************* */

#if defined(__ASSEMBLER__)
#define C0_INX		$0		/* CP0: TLB Index */
#define C0_RAND		$1		/* CP0: TLB Random */
#define C0_TLBLO0	$2		/* CP0: TLB EntryLo0 */
#define C0_TLBLO	C0_TLBLO0	/* CP0: TLB EntryLo0 */
#define C0_TLBLO1	$3		/* CP0: TLB EntryLo1 */
#define C0_CTEXT	$4		/* CP0: Context */
#define C0_PGMASK	$5		/* CP0: TLB PageMask */
#define C0_WIRED	$6		/* CP0: TLB Wired */
#define C0_BADVADDR	$8		/* CP0: Bad Virtual Address */
#define C0_COUNT 	$9		/* CP0: Count */
#define C0_TLBHI	$10		/* CP0: TLB EntryHi */
#define C0_COMPARE	$11		/* CP0: Compare */
#define C0_SR		$12		/* CP0: Processor Status */
#define C0_CAUSE	$13		/* CP0: Exception Cause */
#define C0_EPC		$14		/* CP0: Exception PC */
#define C0_PRID		$15		/* CP0: Processor Revision Indentifier */
#define C0_CONFIG	$16		/* CP0: Config */
#define C0_LLADDR	$17		/* CP0: LLAddr */
#define C0_WATCHLO	$18		/* CP0: WatchpointLo */
#define C0_WATCHHI	$19		/* CP0: WatchpointHi */
#define C0_XCTEXT	$20		/* CP0: XContext */
#define C0_DEBUG   	$23		/* CP0: debug */
#define C0_DEPC    	$24		/* CP0: depc */
#define C0_PERFCONT	$25		/* CP0: Performance counters */
#define C0_ECC		$26		/* CP0: ECC */
#define C0_CACHEERR	$27		/* CP0: CacheErr */
#define C0_TAGLO	$28		/* CP0: TagLo */
#define C0_TAGHI	$29		/* CP0: TagHi */
#define C0_ERREPC	$30		/* CP0: ErrorEPC */
#define C0_DESAVE	$31		/* CP0: JTAG debug exception
					   save register */

#else

#define C0_INX		0		/* CP0: TLB Index */
#define C0_RAND		1		/* CP0: TLB Random */
#define C0_TLBLO0	2		/* CP0: TLB EntryLo0 */
#define C0_TLBLO	C0_TLBLO0	/* CP0: TLB EntryLo0 */
#define C0_TLBLO1	3		/* CP0: TLB EntryLo1 */
#define C0_CTEXT	4		/* CP0: Context */
#define C0_PGMASK	5		/* CP0: TLB PageMask */
#define C0_WIRED	6		/* CP0: TLB Wired */
#define C0_BADVADDR	8		/* CP0: Bad Virtual Address */
#define C0_COUNT 	9		/* CP0: Count */
#define C0_TLBHI	10		/* CP0: TLB EntryHi */
#define C0_COMPARE	11		/* CP0: Compare */
#define C0_SR		12		/* CP0: Processor Status */
#define C0_CAUSE	13		/* CP0: Exception Cause */
#define C0_EPC		14		/* CP0: Exception PC */
#define C0_PRID		15		/* CP0: Processor Revision Indentifier */
#define C0_CONFIG	16		/* CP0: Config */
#define C0_LLADDR	17		/* CP0: LLAddr */
#define C0_WATCHLO	18		/* CP0: WatchpointLo */
#define C0_WATCHHI	19		/* CP0: WatchpointHi */
#define C0_XCTEXT	20		/* CP0: XContext */
#define C0_DEBUG   	23		/* CP0: debug */
#define C0_DEPC    	24		/* CP0: depc */
#define C0_PERFCONT	25		/* CP0: Performance counters */
#define C0_ECC		26		/* CP0: ECC */
#define C0_CACHEERR	27		/* CP0: CacheErr */
#define C0_TAGLO	28		/* CP0: TagLo */
#define C0_TAGHI	29		/* CP0: TagHi */
#define C0_ERREPC	30		/* CP0: ErrorEPC */
#define C0_DESAVE	31		/* CP0: JTAG debug exception
					   save register */

#endif

/* Aliases to match MIPS manuals.  */
#define C0_INDEX        C0_INX
#define C0_RANDOM       C0_RAND
#define C0_ENTRYLO0     C0_TLBLO0
#define C0_ENTRYLO1     C0_TLBLO1
#define C0_CONTEXT      C0_CTEXT
#define C0_PAGEMASK     C0_PGMASK
#define C0_ENTRYHI      C0_TLBHI
#define C0_STATUS       C0_SR
#define C0_XCONTEXT     C0_XCTEXT
#define C0_PERFCNT      C0_PERFCONT
#define C0_ERRCTL       C0_ECC
#define C0_ERROREPC     C0_ERREPC

#ifndef __LANGUAGE_ASSEMBLY

/* Functions to get/set all CP0 registers via inline asms.
   Note that the functions which access 64-bit CP0 register are
   only provided if __mips64 is defined (i.e., if compiling with
   "-mips3", "-mips4", or "-mips64".

   The functions are of the form:
     cp0_get_<name>
     cp0_set_<name> 
   where <name> is the register name as it appears in MIPS
   architecture manuals.

   For example, the functions
     cp0_get_index
     cp0_set_index
   get and set the CP0 Index register.  */

#define	_cp0_get_reg(name, num, sel, type, d)                         \
  static inline type						      \
  cp0_get_ ## name (void)					      \
  {								      \
    type val;							      \
    __asm__ __volatile__ (".set push;"                                \
                          ".set mips64;"                              \
                          d "mfc0 %0, $%1, %2;"                       \
			  ".set pop;"                                 \
			  : "=r"(val) : "i"(num), "i"(sel));          \
    return val;							      \
  }

#define	_cp0_set_reg(name, num, sel, type, d)                         \
  static inline void						      \
  cp0_set_ ## name (type val)					      \
  {								      \
    __asm__ __volatile__ (".set push;"                                \
                          ".set mips64;"                              \
                          d "mtc0 %0, $%1, %2;"                       \
                          ".set pop;"           		      \
                          : : "r"(val), "i"(num), "i"(sel));          \
  }

/* Get and set 32-bit CP0 registers which are treated as unsigned values.  */
#define _cp0_get_reg_u32(name, num, sel) \
  _cp0_get_reg (name, (num), (sel), unsigned int, "")
#define _cp0_set_reg_u32(name, num, sel) \
  _cp0_set_reg (name, (num), (sel), unsigned int, "")

/* Get and set 32-bit CP0 registers which are treated as signed values
   so the high bit can be tested easily.  */
#define _cp0_get_reg_s32(name, num, sel) \
  _cp0_get_reg (name, (num), (sel), int, "")
#define _cp0_set_reg_s32(name, num, sel) \
  _cp0_set_reg (name, (num), (sel), int, "")

#if defined(__mips64)
/* Get and set 64-bit CP0 registers which are treated as unsigned values.
   Note that these functions are only provided if compiling for with
   64-bit GPRs.  */
#define _cp0_get_reg_u64(name, num, sel) \
  _cp0_get_reg (name, (num), (sel), unsigned long long, "d")
#define _cp0_set_reg_u64(name, num, sel) \
  _cp0_set_reg (name, (num), (sel), unsigned long long, "d")
#else
#define _cp0_get_reg_u64(name, num, sel)
#define _cp0_set_reg_u64(name, num, sel)
#endif

/* CP0 register 0: index. */
_cp0_get_reg_s32 (index, C0_INDEX, 0)
_cp0_set_reg_s32 (index, C0_INDEX, 0)

/* CP0 register 1: random. */
_cp0_get_reg_u32 (random, C0_RANDOM, 0)

/* CP0 register 2: entrylo0. */
_cp0_get_reg_u64 (entrylo0, C0_ENTRYLO0, 0)
_cp0_set_reg_u64 (entrylo0, C0_ENTRYLO0, 0)

/* CP0 register 3: entrylo1. */
_cp0_get_reg_u64 (entrylo1, C0_ENTRYLO1, 0)
_cp0_set_reg_u64 (entrylo1, C0_ENTRYLO1, 0)

/* CP0 register 4: context. */
_cp0_get_reg_u64 (context, C0_CONTEXT, 0)
_cp0_set_reg_u64 (context, C0_CONTEXT, 0)

/* CP0 register 5: pagemask. */
_cp0_get_reg_u32 (pagemask, C0_PAGEMASK, 0)
_cp0_set_reg_u32 (pagemask, C0_PAGEMASK, 0)

/* CP0 register 6: wired. */
_cp0_get_reg_u32 (wired, C0_WIRED, 0)
_cp0_set_reg_u32 (wired, C0_WIRED, 0)

/* CP0 register 7: reserved. */

/* CP0 register 8: badvaddr. */
_cp0_get_reg_u64 (badvaddr, C0_BADVADDR, 0)

/* CP0 register 9: count. */
_cp0_get_reg_u32 (count, C0_COUNT, 0)
_cp0_set_reg_u32 (count, C0_COUNT, 0)

/* CP0 register 10: entryhi. */
_cp0_get_reg_u64 (entryhi, C0_ENTRYHI, 0)
_cp0_set_reg_u64 (entryhi, C0_ENTRYHI, 0)

/* CP0 register 11: compare. */
_cp0_get_reg_u32 (compare, C0_COMPARE, 0)
_cp0_set_reg_u32 (compare, C0_COMPARE, 0)

/* CP0 register 12: status. */
_cp0_get_reg_u32 (status, C0_STATUS, 0)
_cp0_set_reg_u32 (status, C0_STATUS, 0)

/* CP0 register 13: cause. */
_cp0_get_reg_u32 (cause, C0_CAUSE, 0)
_cp0_set_reg_u32 (cause, C0_CAUSE, 0)

/* CP0 register 14: epc. */
_cp0_get_reg_u64 (epc, C0_EPC, 0)
_cp0_set_reg_u64 (epc, C0_EPC, 0)

/* CP0 register 15: prid. */
_cp0_get_reg_u32 (prid, C0_PRID, 0)

/* CP0 register 16: config. */
_cp0_get_reg_u32 (config, C0_CONFIG, 0)
_cp0_set_reg_u32 (config, C0_CONFIG, 0)

/* CP0 register 16 sel 1: config1. */
_cp0_get_reg_u32 (config1, C0_CONFIG, 1)

/* CP0 register 16 sel 2: config2. */
_cp0_get_reg_u32 (config2, C0_CONFIG, 2)

/* CP0 register 16 sel 3: config3. */
_cp0_get_reg_u32 (config3, C0_CONFIG, 3)

/* CP0 register 17: lladdr. */
_cp0_get_reg_u64 (lladdr, C0_LLADDR, 0)

/* CP0 register 18: watchlo. */
_cp0_get_reg_u64 (watchlo, C0_WATCHLO, 0)
_cp0_set_reg_u64 (watchlo, C0_WATCHLO, 0)

/* CP0 register 18 sel 1: watchlo1. */
_cp0_get_reg_u64 (watchlo1, C0_WATCHLO, 1)
_cp0_set_reg_u64 (watchlo1, C0_WATCHLO, 1)

/* CP0 register 19: watchhi. */
_cp0_get_reg_u32 (watchhi, C0_WATCHHI, 0)
_cp0_set_reg_u32 (watchhi, C0_WATCHHI, 0)

/* CP0 register 19 sel 1: watchhi1. */
_cp0_get_reg_u32 (watchhi1, C0_WATCHHI, 1)
_cp0_set_reg_u32 (watchhi1, C0_WATCHHI, 1)

/* CP0 register 20: xcontext. */
_cp0_get_reg_u64 (xcontext, C0_XCONTEXT, 0)
_cp0_set_reg_u64 (xcontext, C0_XCONTEXT, 0)

/* CP0 register 21: reserved. */

/* CP0 register 22: implementation dependent use. */

/* CP0 register 23: debug. */
_cp0_get_reg_u32 (debug, C0_DEBUG, 0)
_cp0_set_reg_u32 (debug, C0_DEBUG, 0)
/* CP0 register 23 sel 3: edebug. */
_cp0_get_reg_u32 (edebug, C0_DEBUG, 3)
_cp0_set_reg_u32 (edebug, C0_DEBUG, 3)

/* CP0 register 24: depc. */
_cp0_get_reg_u64 (depc, C0_DEPC, 0)
_cp0_set_reg_u64 (depc, C0_DEPC, 0)

/* CP0 register 25: perfcnt. */
_cp0_get_reg_u32 (perfcnt, C0_PERFCNT, 0)
_cp0_set_reg_u32 (perfcnt, C0_PERFCNT, 0)

/* CP0 register 25 sel 1: perfcnt1. */
_cp0_get_reg_u32 (perfcnt1, C0_PERFCNT, 1)
_cp0_set_reg_u32 (perfcnt1, C0_PERFCNT, 1)

/* CP0 register 25 sel 2: perfcnt2. */
_cp0_get_reg_u32 (perfcnt2, C0_PERFCNT, 2)
_cp0_set_reg_u32 (perfcnt2, C0_PERFCNT, 2)

/* CP0 register 25 sel 3: perfcnt3. */
_cp0_get_reg_u32 (perfcnt3, C0_PERFCNT, 3)
_cp0_set_reg_u32 (perfcnt3, C0_PERFCNT, 3)

/* CP0 register 25 sel 4: perfcnt4. */
_cp0_get_reg_u32 (perfcnt4, C0_PERFCNT, 4)
_cp0_set_reg_u32 (perfcnt4, C0_PERFCNT, 4)

/* CP0 register 25 sel 5: perfcnt5. */
_cp0_get_reg_u32 (perfcnt5, C0_PERFCNT, 5)
_cp0_set_reg_u32 (perfcnt5, C0_PERFCNT, 5)

/* CP0 register 25 sel 6: perfcnt6. */
_cp0_get_reg_u32 (perfcnt6, C0_PERFCNT, 6)
_cp0_set_reg_u32 (perfcnt6, C0_PERFCNT, 6)

/* CP0 register 25 sel 7: perfcnt7. */
_cp0_get_reg_u32 (perfcnt7, C0_PERFCNT, 7)
_cp0_set_reg_u32 (perfcnt7, C0_PERFCNT, 7)

/* CP0 register 26: errctl. */
_cp0_get_reg_u32 (errctl, C0_ERRCTL, 0)
_cp0_get_reg_u32 (buserr_pa, C0_ERRCTL, 1)

/* CP0 register 27: cacheerr_i. */
_cp0_get_reg_u32 (cacheerr_i, C0_CACHEERR, 0)

/* CP0 register 27 sel 1: cacheerr_d. */
_cp0_get_reg_u32 (cacheerr_d, C0_CACHEERR, 1)

/* CP0 register 27 sel 3: cacheerr_d_pa. */
_cp0_get_reg_u32 (cacheerr_d_pa, C0_CACHEERR, 3)

/* CP0 register 28: taglo_i. */
_cp0_get_reg_u64 (taglo_i, C0_TAGLO, 0)
_cp0_set_reg_u64 (taglo_i, C0_TAGLO, 0)

/* CP0 register 28 sel 1: datalo_i. */
_cp0_get_reg_u64 (datalo_i, C0_TAGLO, 1)

/* CP0 register 28 sel 2: taglo_d. */
_cp0_get_reg_u64 (taglo_d, C0_TAGLO, 2)
_cp0_set_reg_u64 (taglo_d, C0_TAGLO, 2)

/* CP0 register 28 sel 3: datalo_d. */
_cp0_get_reg_u64 (datalo_d, C0_TAGLO, 3)

/* CP0 register 29: taghi_i. */
_cp0_get_reg_u64 (taghi_i, C0_TAGHI, 0)
_cp0_set_reg_u64 (taghi_i, C0_TAGHI, 0)

/* CP0 register 29 sel 1: datahi_i. */
_cp0_get_reg_u64 (datahi_i, C0_TAGHI, 1)

/* CP0 register 29 sel 2: taghi_d. */
_cp0_get_reg_u64 (taghi_d, C0_TAGHI, 2)
_cp0_set_reg_u64 (taghi_d, C0_TAGHI, 2)

/* CP0 register 29 sel 3: datahi_d. */
_cp0_get_reg_u64 (datahi_d, C0_TAGHI, 3)

/* CP0 register 30: errorepc. */
_cp0_get_reg_u64 (errorepc, C0_ERROREPC, 0)
_cp0_set_reg_u64 (errorepc, C0_ERROREPC, 0)

/* CP0 register 31: desave. */
_cp0_get_reg_u64 (desave, C0_DESAVE, 0)
_cp0_set_reg_u64 (desave, C0_DESAVE, 0)

#endif /* __LANGUAGE_ASSEMBLY */

/*  *********************************************************************
    *  CP1 (floating point) control registers
    ********************************************************************* */

#define FPA_IRR		0		/* CP1: Implementation/Revision */
#define FPA_CSR		31		/* CP1: Control/Status */

/*  *********************************************************************
    *  Macros for generating assembly language routines
    ********************************************************************* */

#if defined(__ASSEMBLER__)

/* global leaf function (does not call other functions) */
#define LEAF(name)		\
  	.globl	name;		\
  	.ent	name;		\
name:

/* global alternate entry to (local or global) leaf function */
#define XLEAF(name)		\
  	.globl	name;		\
  	.aent	name;		\
name:

/* end of a global function */
#define END(name)		\
  	.size	name,.-name;	\
  	.end	name

/* local leaf function (does not call other functions) */
#define SLEAF(name)		\
  	.ent	name;		\
name:

/* local alternate entry to (local or global) leaf function */
#define SXLEAF(name)		\
  	.aent	name;		\
name:

/* end of a local function */
#define SEND(name)		\
  	END(name)

/* define & export a symbol */
#define EXPORT(name)		\
  	.globl name;		\
name:

/* import a symbol */
#define	IMPORT(name, size)	\
	.extern	name,size

/* define a zero-fill common block (BSS if not overridden) with a global name */
#define COMM(name,size)		\
	.comm	name,size

/* define a zero-fill common block (BSS if not overridden) with a local name */
#define LCOMM(name,size)		\
  	.lcomm	name,size

#endif


/* Floating-Point Control register bits */
#define CSR_C		0x00800000
#define CSR_EXC		0x0003f000
#define CSR_EE		0x00020000
#define CSR_EV		0x00010000
#define CSR_EZ		0x00008000
#define CSR_EO		0x00004000
#define CSR_EU		0x00002000
#define CSR_EI		0x00001000
#define CSR_TV		0x00000800
#define CSR_TZ		0x00000400
#define CSR_TO		0x00000200
#define CSR_TU		0x00000100
#define CSR_TI		0x00000080
#define CSR_SV		0x00000040
#define CSR_SZ		0x00000020
#define CSR_SO		0x00000010
#define CSR_SU		0x00000008
#define CSR_SI		0x00000004
#define CSR_RM		0x00000003

/* Status Register */

#define S_SR_CUMASK	28				/* coprocessor usable bits */
#define M_SR_CUMASK	_MM_MAKEMASK(4,S_SR_CUMASK)
#define G_SR_CUMASK(x)	_MM_GETVALUE(x,S_SR_CUMASK,M_SR_CUMASK)

#define M_SR_CU3	_MM_MAKEMASK1(31)	/* coprocessor 3 usable */
#define M_SR_CU2	_MM_MAKEMASK1(30)	/* coprocessor 2 usable */
#define M_SR_CU1	_MM_MAKEMASK1(29)	/* coprocessor 1 usable */
#define M_SR_CU0	_MM_MAKEMASK1(28)	/* coprocessor 0 usable */

#define S_SR_RP		27			/* reduced power mode */
#define M_SR_RP		_MM_MAKEMASK1(27)	/* reduced power mode */
#define G_SR_RP(x)	_MM_GETVALUE(x,S_SR_RP,M_SR_RP)

#define S_SR_FR		26			/* fpu regs any data */
#define M_SR_FR		_MM_MAKEMASK1(26)	/* fpu regs any data */
#define G_SR_FR(x)	_MM_GETVALUE(x,S_SR_FR,M_SR_FR)

#define S_SR_RE		25			/* reverse endian */
#define M_SR_RE		_MM_MAKEMASK1(25)	/* reverse endian */
#define G_SR_RE(x)	_MM_GETVALUE(x,S_SR_RE,M_SR_RE)

#define S_SR_MX		24			/* MDMX */
#define M_SR_MX		_MM_MAKEMASK1(24)	/* MDMX */
#define G_SR_MX(x)	_MM_GETVALUE(x,S_SR_MX,M_SR_MX)

#define S_SR_PX		23			/* 64-bit ops in user mode */
#define M_SR_PX		_MM_MAKEMASK1(23)	/* 64-bit ops in user mode */
#define G_SR_PX(x)	_MM_GETVALUE(x,S_SR_PX,M_SR_PX)

#define S_SR_BEV	22			/* boot exception vectors */
#define M_SR_BEV	_MM_MAKEMASK1(22)	/* boot exception vectors */
#define G_SR_BEV(x)	_MM_GETVALUE(x,S_SR_BEV,M_SR_BEV)

#define S_SR_TS		21			/* TLB is shut down */
#define M_SR_TS		_MM_MAKEMASK1(21)	/* TLB is shut down */
#define G_SR_TS(x)	_MM_GETVALUE(x,S_SR_TS,M_SR_TS)

#define S_SR_SR		20			/* soft reset */
#define M_SR_SR		_MM_MAKEMASK1(20)	/* soft reset */
#define G_SR_SR(x)	_MM_GETVALUE(x,S_SR_SR,M_SR_SR)

#define S_SR_NMI	19			/* nonmaskable interrupt */
#define M_SR_NMI	_MM_MAKEMASK1(19)	/* nonmaskable interrupt */
#define G_SR_NMI(x)	_MM_GETVALUE(x,S_SR_NMI,M_SR_NMI)

#define S_SR_IMASK	8			/* all interrupt mask bits */
#define M_SR_IMASK	_MM_MAKEMASK(8,8)	/* all interrupt mask bits */
#define G_SR_IMASK(x)	_MM_GETVALUE(x,S_SR_IMASK,M_SR_IMASK)

#define M_SR_IBIT8	_MM_MAKEMASK1(15)	/* individual bits */
#define M_SR_IBIT7	_MM_MAKEMASK1(14)
#define M_SR_IBIT6	_MM_MAKEMASK1(13)
#define M_SR_IBIT5	_MM_MAKEMASK1(12)
#define M_SR_IBIT4	_MM_MAKEMASK1(11)
#define M_SR_IBIT3	_MM_MAKEMASK1(10)
#define M_SR_IBIT2	_MM_MAKEMASK1(9)
#define M_SR_IBIT1	_MM_MAKEMASK1(8)

#define M_SR_IMASK8	0			/* masks for nested int levels */
#define M_SR_IMASK7	_MM_MAKEMASK(1,15)
#define M_SR_IMASK6	_MM_MAKEMASK(2,14)
#define M_SR_IMASK5	_MM_MAKEMASK(3,13)
#define M_SR_IMASK4	_MM_MAKEMASK(4,12)
#define M_SR_IMASK3	_MM_MAKEMASK(5,11)
#define M_SR_IMASK2	_MM_MAKEMASK(6,10)
#define M_SR_IMASK1	_MM_MAKEMASK(7,9)
#define M_SR_IMASK0	_MM_MAKEMASK(8,8)

#define S_SR_KX		7			/* 64-bit access for kernel */
#define M_SR_KX		_MM_MAKEMASK1(7)	/* 64-bit access for kernel */
#define G_SR_KX(x)	_MM_GETVALUE(x,S_SR_KX,M_SR_KX)

#define S_SR_SX		6			/* .. for supervisor */
#define M_SR_SX		_MM_MAKEMASK1(6)	/* .. for supervisor */
#define G_SR_SX(x)	_MM_GETVALUE(x,S_SR_SX,M_SR_SX)

#define S_SR_UX		5			/* .. for user */
#define M_SR_UX		_MM_MAKEMASK1(5)	/* .. for user */
#define G_SR_UX(x)	_MM_GETVALUE(x,S_SR_UX,M_SR_UX)

#define S_SR_KSU	3			/* base operating mode mode */
#define M_SR_KSU	_MM_MAKEMASK(2,S_SR_KSU)
#define V_SR_KSU(x)	_MM_MAKEVALUE(x,S_SR_KSU)
#define G_SR_KSU(x)	_MM_GETVALUE(x,S_SR_KSU,M_SR_KSU)
#define K_SR_KSU_KERNEL	0
#define K_SR_KSU_SUPR	1
#define K_SR_KSU_USER	2

#define M_SR_UM		_MM_MAKEMASK1(4)

#define S_SR_ERL	2
#define M_SR_ERL	_MM_MAKEMASK1(2)
#define G_SR_ERL(x)	_MM_GETVALUE(x,S_SR_ERL,M_SR_ERL)

#define S_SR_EXL	1
#define M_SR_EXL	_MM_MAKEMASK1(1)
#define G_SR_EXL(x)	_MM_GETVALUE(x,S_SR_EXL,M_SR_EXL)

#define S_SR_IE		0
#define M_SR_IE		_MM_MAKEMASK1(0)
#define G_SR_IE(x)	_MM_GETVALUE(x,S_SR_IE,M_SR_IE)

/* 
 * Cause Register 
 */
#define M_CAUSE_BD	_MM_MAKEMASK1(31) /* exception in BD slot */

#define S_CAUSE_CE	28		/* coprocessor error */
#define M_CAUSE_CE	_MM_MAKEMASK(2,S_CAUSE_CE)
#define V_CAUSE_CE(x)	_MM_MAKEVALUE(x,S_CAUSE_CE)
#define G_CAUSE_CE(x)	_MM_GETVALUE(x,S_CAUSE_CE,M_CAUSE_CE)

#define M_CAUSE_IV	_MM_MAKEMASK1(23) /* special interrupt */
#define M_CAUSE_WP      _MM_MAKEMASK1(22) /* watch interrupt deferred */

#define S_CAUSE_IPMASK	8
#define M_CAUSE_IPMASK	_MM_MAKEMASK(8,S_CAUSE_IPMASK)
#define M_CAUSE_IP8	_MM_MAKEMASK1(15)	/* hardware interrupts */
#define M_CAUSE_IP7	_MM_MAKEMASK1(14)
#define M_CAUSE_IP6	_MM_MAKEMASK1(13)
#define M_CAUSE_IP5	_MM_MAKEMASK1(12)
#define M_CAUSE_IP4	_MM_MAKEMASK1(11)
#define M_CAUSE_IP3	_MM_MAKEMASK1(10)
#define M_CAUSE_SW2	_MM_MAKEMASK1(9)	/* software interrupts */
#define M_CAUSE_SW1	_MM_MAKEMASK1(8)

#define S_CAUSE_EXC	2
#define M_CAUSE_EXC	_MM_MAKEMASK(5,S_CAUSE_EXC)
#define V_CAUSE_EXC(x)	_MM_MAKEVALUE(x,S_CAUSE_EXC)
#define G_CAUSE_EXC(x)	_MM_GETVALUE(x,S_CAUSE_EXC,M_CAUSE_EXC)

/* Exception Code */
#define K_CAUSE_EXC_INT		0	/* External interrupt */
#define K_CAUSE_EXC_MOD		1	/* TLB modification */
#define K_CAUSE_EXC_TLBL	2    	/* TLB miss (Load or Ifetch) */
#define K_CAUSE_EXC_TLBS	3	/* TLB miss (Save) */
#define K_CAUSE_EXC_ADEL	4    	/* Address error (Load or Ifetch) */
#define K_CAUSE_EXC_ADES	5	/* Address error (Save) */
#define K_CAUSE_EXC_IBE		6	/* Bus error (Ifetch) */
#define K_CAUSE_EXC_DBE		7	/* Bus error (data load or store) */
#define K_CAUSE_EXC_SYS		8	/* System call */
#define K_CAUSE_EXC_BP		9	/* Break point */
#define K_CAUSE_EXC_RI		10	/* Reserved instruction */
#define K_CAUSE_EXC_CPU		11	/* Coprocessor unusable */
#define K_CAUSE_EXC_OVF		12	/* Arithmetic overflow */
#define K_CAUSE_EXC_TRAP	13	/* Trap exception */
#define K_CAUSE_EXC_VCEI	14	/* Virtual Coherency Exception (I) */
#define K_CAUSE_EXC_FPE		15	/* Floating Point Exception */
#define K_CAUSE_EXC_CP2		16	/* Cp2 Exception */
#define K_CAUSE_EXC_WATCH	23	/* Watchpoint exception */
#define K_CAUSE_EXC_VCED	31	/* Virtual Coherency Exception (D) */

#define	K_NTLBENTRIES	64

#define HI_HALF(x)	((x) >> 16)
#define LO_HALF(x)	((x) & 0xffff)

/* FPU stuff */

#if defined(__ASSEMBLER__)
#define C1_CSR		$31
#define C1_FRID		$0
#else
#define C1_CSR		31
#define C1_FRID		0
#endif

#define S_FCSR_CAUSE	12
#define M_FCSR_CAUSE	_MM_MAKEMASK(5,S_FCSR_CAUSE)
#define V_FCSR_CAUSE(x)	_MM_MAKEVALUE(x,S_FCSR_CAUSE)
#define G_FCSR_CAUSE(x)	_MM_GETVALUE(x,S_FCSR_CAUSE,M_FCSR_CAUSE)

#define S_FCSR_ENABLES	7
#define M_FCSR_ENABLES	_MM_MAKEMASK(5,S_FCSR_ENABLES)
#define V_FCSR_ENABLES(x) _MM_MAKEVALUE(x,S_FCSR_ENABLES)
#define G_FCSR_ENABLES(x) _MM_GETVALUE(x,S_FCSR_ENABLES,M_FCSR_ENABLES)

#define S_FCSR_FLAGS	2
#define M_FCSR_FLAGS	_MM_MAKEMASK(5,S_FCSR_FLAGS)
#define V_FCSR_FLAGS(x)	_MM_MAKEVALUE(x,S_FCSR_FLAGS)
#define G_FCSR_FLAGS(x)	_MM_GETVALUE(x,S_FCSR_FLAGS,M_FCSR_FLAGS)


/*
 * MIPS64 Config Register (select 0)
 */
#define S_CFG_CFG1	31			/* Config1 */
#define M_CFG_CFG1	_MM_MAKEMASK1(31)	/* config1 select1 is impl */
#define G_CFG_CFG1(x)	_MM_GETVALUE(x,S_CFG_CFG1,M_CFG_CFG1)
#define S_CFG_BE	15			/* Endian mode */
#define M_CFG_BE        _MM_MAKEMASK1(15)	/* big-endian mode */
#define G_CFG_BE(x)	_MM_GETVALUE(x,S_CFG_BE,M_CFG_BE)

#define S_CFG_MPV	16			/* Multi proc. vector offset */
#define M_CFG_MPV        _MM_MAKEMASK(4,S_CFG_MPV)
#define V_CFG_MPV(x)	_MM_MAKEVALUE(x,S_CFG_MPV)
#define G_CFG_MPV(x)	_MM_GETVALUE(x,S_CFG_MPV,M_CFG_MPV)

#define S_CFG_AT	13			/* Architecture Type */
#define M_CFG_AT	_MM_MAKEMASK(2,S_CFG_AT)
#define V_CFG_AT(x)	_MM_MAKEVALUE(x,S_CFG_AT)
#define G_CFG_AT(x)	_MM_GETVALUE(x,S_CFG_AT,M_CFG_AT)
#define K_CFG_AT_MIPS32	0
#define K_CFG_AT_MIPS64_32 1
#define K_CFG_AT_MIPS64	2

#define S_CFG_AR	10			/* Architecture Revision */
#define M_CFG_AR        _MM_MAKEMASK(3,S_CFG_AR)
#define V_CFG_AR(x)	_MM_MAKEVALUE(x,S_CFG_AR)
#define G_CFG_AR(x)	_MM_GETVALUE(x,S_CFG_AR,M_CFG_AR)
#define K_CFG_AR_REV1	0

#define S_CFG_MMU	7			/* MMU Type */
#define M_CFG_MMU       _MM_MAKEMASK(3,S_CFG_MMU)
#define V_CFG_MMU(x)	_MM_MAKEVALUE(x,S_CFG_MMU)
#define G_CFG_MMU(x)	_MM_GETVALUE(x,S_CFG_MMU,M_CFG_MMU)
#define K_CFG_MMU_NONE	0
#define K_CFG_MMU_TLB	1
#define K_CFG_MMU_BAT	2
#define K_CFG_MMU_FIXED	3

#define S_CFG_K0COH	0			/* K0seg coherency */
#define M_CFG_K0COH	_MM_MAKEMASK(3,S_CFG_K0COH)
#define V_CFG_K0COH(x)	_MM_MAKEVALUE(x,S_CFG_K0COH)
#define G_CFG_K0COH(x)	_MM_GETVALUE(x,S_CFG_K0COH,M_CFG_K0COH)
#define K_CFG_K0COH_UNCACHED	2
#define K_CFG_K0COH_CACHEABLE	3
#define K_CFG_K0COH_COHERENT	5

/*
 * MIPS64 Config Register (select 1)
 */

#define M_CFG_CFG2	_MM_MAKEMASK1(31)	/* config2 select2 is impl */

#define S_CFG_MMUSIZE	25
#define M_CFG_MMUSIZE	_MM_MAKEMASK(6,S_CFG_MMUSIZE)
#define G_CFG_MMUSIZE(x) _MM_GETVALUE(x,S_CFG_MMUSIZE,M_CFG_MMUSIZE)

#define S_CFG_IS	22
#define M_CFG_IS	_MM_MAKEMASK(3,S_CFG_IS)
#define V_CFG_IS(x)	_MM_MAKEVALUE(x,S_CFG_IS)
#define G_CFG_IS(x)	_MM_GETVALUE(x,S_CFG_IS,M_CFG_IS)

#define S_CFG_IL	19
#define M_CFG_IL	_MM_MAKEMASK(3,S_CFG_IL)
#define V_CFG_IL(x)	_MM_MAKEVALUE(x,S_CFG_IL)
#define G_CFG_IL(x)	_MM_GETVALUE(x,S_CFG_IL,M_CFG_IL)

#define S_CFG_IA	16
#define M_CFG_IA	_MM_MAKEMASK(3,S_CFG_IA)
#define V_CFG_IA(x)	_MM_MAKEVALUE(x,S_CFG_IA)
#define G_CFG_IA(x)	_MM_GETVALUE(x,S_CFG_IA,M_CFG_IA)

#define S_CFG_DS	13
#define M_CFG_DS	_MM_MAKEMASK(3,S_CFG_DS)
#define V_CFG_DS(x)	_MM_MAKEVALUE(x,S_CFG_DS)
#define G_CFG_DS(x)	_MM_GETVALUE(x,S_CFG_DS,M_CFG_DS)

#define S_CFG_DL	10
#define M_CFG_DL	_MM_MAKEMASK(3,S_CFG_DL)
#define V_CFG_DL(x)	_MM_MAKEVALUE(x,S_CFG_DL)
#define G_CFG_DL(x)	_MM_GETVALUE(x,S_CFG_DL,M_CFG_DL)

#define S_CFG_DA	7
#define M_CFG_DA	_MM_MAKEMASK(3,S_CFG_DA)
#define V_CFG_DA(x)	_MM_MAKEVALUE(x,S_CFG_DA)
#define G_CFG_DA(x)	_MM_GETVALUE(x,S_CFG_DA,M_CFG_DA)

#define S_CFG_PC	4			/* perf ctrs present */
#define M_CFG_PC	_MM_MAKEMASK1(4)	/* perf ctrs present */
#define G_CFG_PC(x)	_MM_GETVALUE(x,S_CFG_PC,M_CFG_PC)

#define S_CFG_WR	3			/* watch regs present */
#define M_CFG_WR	_MM_MAKEMASK1(3)	/* watch regs present */
#define G_CFG_WR(x)	_MM_GETVALUE(x,S_CFG_WR,M_CFG_WR)

#define S_CFG_CA	2			/* MIPS16 present */
#define M_CFG_CA	_MM_MAKEMASK1(2)	/* MIPS16 present */
#define G_CFG_CA(x)	_MM_GETVALUE(x,S_CFG_CA,M_CFG_CA)

#define S_CFG_EP	1			/* EJTAG present */
#define M_CFG_EP	_MM_MAKEMASK1(1)	/* EJTAG present */
#define G_CFG_EP(x)	_MM_GETVALUE(x,S_CFG_EP,M_CFG_EP)

#define S_CFG_FP	0			/* FPU present */
#define M_CFG_FP	_MM_MAKEMASK1(0)	/* FPU present */
#define G_CFG_FP(x)	_MM_GETVALUE(x,S_CFG_FP,M_CFG_FP)



/* 
 * Primary Cache TagLo 
 */

#define S_TAGLO_PTAG	8
#define M_TAGLO_PTAG 	_MM_MAKEMASK(56,S_TAGLO_PTAG)

#define S_TAGLO_PSTATE	6
#define M_TAGLO_PSTATE	_MM_MAKEMASK(2,S_TAGLO_PSTATE)
#define V_TAGLO_PSTATE(x) _MM_MAKEVALUE(x,S_TAGLO_PSTATE)
#define G_TAGLO_PSTATE(x) _MM_GETVALUE(x,S_TAGLO_PSTATE,M_TAGLO_PSTATE)
#define K_TAGLO_PSTATE_INVAL		0
#define K_TAGLO_PSTATE_SHARED		1
#define K_TAGLO_PSTATE_CLEAN_EXCL	2
#define K_TAGLO_PSTATE_DIRTY_EXCL	3

#define M_TAGLO_LOCK	_MM_MAKEMASK1(5)
#define M_TAGLO_PARITY	_MM_MAKEMASK1(0)


/*
 * CP0 CacheErr register
 */
#define M_CERR_DATA	_MM_MAKEMASK1(31)	/* err in D space */
#define M_CERR_SCACHE	_MM_MAKEMASK1(30)	/* err in l2, not l1 */
#define M_CERR_DERR	_MM_MAKEMASK1(29)	/* data error */
#define M_CERR_TERR	_MM_MAKEMASK1(28)	/* tag error */
#define M_CERR_EXTRQ	_MM_MAKEMASK1(27)	/* external req caused err */
#define M_CERR_BPAR	_MM_MAKEMASK1(26)	/* bus parity err */
#define M_CERR_ADATA	_MM_MAKEMASK1(25)	/* additional data */
#define M_CERR_IDX	_MM_MAKEMASK(22,0)



/*
 * Primary Cache operations
 */
#define Index_Invalidate_I               0x0         /* 0       0 */
#define Index_Writeback_Inv_D            0x1         /* 0       1 */
#define Index_Invalidate_SI              0x2         /* 0       2 */
#define Index_Writeback_Inv_SD           0x3         /* 0       3 */
#define Index_Load_Tag_I                 0x4         /* 1       0 */
#define Index_Load_Tag_D                 0x5         /* 1       1 */
#define Index_Load_Tag_SI                0x6         /* 1       2 */
#define Index_Load_Tag_SD                0x7         /* 1       3 */
#define Index_Store_Tag_I                0x8         /* 2       0 */
#define Index_Store_Tag_D                0x9         /* 2       1 */
#define Index_Store_Tag_SI               0xA         /* 2       2 */
#define Index_Store_Tag_SD               0xB         /* 2       3 */
#define Create_Dirty_Exc_D               0xD         /* 3       1 */
#define Create_Dirty_Exc_SD              0xF         /* 3       3 */
#define Hit_Invalidate_I                 0x10        /* 4       0 */
#define Hit_Invalidate_D                 0x11        /* 4       1 */
#define Hit_Invalidate_SI                0x12        /* 4       2 */
#define Hit_Invalidate_SD                0x13        /* 4       3 */
#define Fill_I                           0x14        /* 5       0 */
#define Hit_Writeback_Inv_D              0x15        /* 5       1 */
#define Hit_Writeback_Inv_SD             0x17        /* 5       3 */
#define Hit_Writeback_I                  0x18        /* 6       0 */
#define Hit_Writeback_D                  0x19        /* 6       1 */
#define Hit_Writeback_SD                 0x1B        /* 6       3 */
#define Hit_Set_Virtual_SI               0x1E        /* 7       2 */
#define Hit_Set_Virtual_SD               0x1F        /* 7       3 */

/* Watchpoint Register */
#define M_WATCH_PA		0xfffffff8
#define M_WATCH_R		0x00000002
#define M_WATCH_W		0x00000001


/* TLB entries */
#define S_TLBHI_ASID            0
#define M_TLBHI_ASID		_MM_MAKEMASK(8,S_TLBHI_ASID)
#define V_TLBHI_ASID(x) 	_MM_MAKEVALUE(x,S_TLBHI_ASID)
#define G_TLBHI_ASID(x) 	_MM_GETVALUE(x,S_TLBLO_ASID,M_TLBLO_ASID)

/* SEGBITS = 44 on sb1 */
#define S_TLBHI_VPN2		13
#define M_TLBHI_VPN2		_MM_MAKEMASK(31,S_TLBHI_VPN2)
#define V_TLBHI_VPN2(x) 	_MM_MAKEVALUE(x,S_TLBHI_VPN2)
#define G_TLBHI_VPN2(x) 	_MM_GETVALUE(x,S_TLBHI_VPN2,M_TLBHI_VPN2)

#define S_TLBLO_G		0
#define M_TLBLO_G		_MM_MAKEMASK1(S_TLBLO_G)
#define V_TLBLO_G(x) 	        _MM_MAKEVALUE(x,S_TLBLO_G)
#define G_TLBLO_G(x) 	        _MM_GETVALUE(x,S_TLBLO_G,M_TLBLO_G)

#define S_TLBLO_V		1
#define M_TLBLO_V		_MM_MAKEMASK1(S_TLBLO_V)
#define V_TLBLO_V(x) 	        _MM_MAKEVALUE(x,S_TLBLO_V)
#define G_TLBLO_V(x) 	        _MM_GETVALUE(x,S_TLBLO_V,M_TLBLO_V)

#define S_TLBLO_D		2
#define M_TLBLO_D		_MM_MAKEMASK1(S_TLBLO_D)
#define V_TLBLO_D(x) 	        _MM_MAKEVALUE(x,S_TLBLO_D)
#define G_TLBLO_D(x) 	        _MM_GETVALUE(x,S_TLBLO_D,M_TLBLO_D)

#define S_TLBLO_CALG		3
#define M_TLBLO_CALG		_MM_MAKEMASK(3,S_TLBLO_CALG)
#define V_TLBLO_CALG(x) 	_MM_MAKEVALUE(x,S_TLBLO_CALG)
#define G_TLBLO_CALG(x) 	_MM_GETVALUE(x,S_TLBLO_CALG,M_TLBLO_CALG)

/* PABITS = 40 on sb1 */
#define S_TLBLO_PFNMASK		6
#define M_TLBLO_PFNMASK		_MM_MAKEMASK(28,S_TLBLO_PFNMASK)
#define V_TLBLO_PFNMASK(x) 	_MM_MAKEVALUE(x,S_TLBLO_PFNMASK)
#define G_TLBLO_PFNMASK(x) 	_MM_GETVALUE(x,S_TLBLO_PFNMASK,M_TLBLO_PFNMASK)

/* support 4KB - 64MB for pass2 and beyond (14bits) */
#define S_TLB_PGMSK             13
#define M_TLB_PGMSK		_MM_MAKEMASK(14,S_TLB_PGMSK)
#define V_TLB_PGMSK(x) 	        _MM_MAKEVALUE(x,S_TLB_PGMSK)
#define G_TLB_PGMSK(x) 	        _MM_GETVALUE(x,S_TLB_PGMSK,M_TLB_PGMSK)

#define K_CALG_COH_EXCL1_NOL2	0
#define K_CALG_COH_SHRL1_NOL2	1
#define K_CALG_UNCACHED		2
#define K_CALG_NONCOHERENT	3
#define K_CALG_COH_EXCL		4
#define K_CALG_COH_SHAREABLE	5
#define K_CALG_NOTUSED		6
#define K_CALG_UNCACHED_ACCEL	7



#endif /* _SB_MIPS_H */
