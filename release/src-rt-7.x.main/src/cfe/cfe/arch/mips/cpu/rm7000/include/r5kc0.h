/*
 * r5kc0.h : base R5000 coprocessor 0 definitions
 * Copyright (c) 1997 Algorithmics Ltd.
 */

#ifndef _R5KC0_H_
#ifdef __cplusplus
extern "C" {
#endif
#define _R5KC0_H_

/*
 * R5000 Exception Codes
 */
#define EXC_INTR	0	/* interrupt */
#define EXC_MOD		1	/* tlb modification */
#define EXC_TLBL	2	/* tlb miss (load/i-fetch) */
#define EXC_TLBS	3	/* tlb miss (store) */
#define EXC_ADEL	4	/* address error (load/i-fetch) */
#define EXC_ADES	5	/* address error (store) */
#define EXC_IBE		6	/* bus error (i-fetch) */
#define EXC_DBE		7	/* data bus error (load/store) */
#define EXC_SYS		8	/* system call */
#define EXC_BP		9	/* breakpoint */
#define EXC_RI		10	/* reserved instruction */
#define EXC_CPU		11	/* coprocessor unusable */
#define EXC_OVF		12	/* integer overflow */
#define EXC_TRAP	13	/* trap exception */
#define EXC_FPE		15	/* floating point exception */
#if #cpu(rm52xx)
#define EXC_WATCH	23	/* watchpoint - not on all variants */
#endif

/*
 * R5000 Cause Register 
 */
#define CR_BD		0x80000000	/* branch delay */
#define CR_CEMASK	0x30000000      /* coprocessor used */
#define CR_CESHIFT	28
#define CR_IV		0x00800000	/* interrupt vector enable */

/* interrupt pending bits */
#define CR_SINT0	0x00000100 	/* s/w interrupt 0 */
#define CR_SINT1	0x00000200	/* s/w interrupt 1 */
#define CR_HINT0	0x00000400	/* h/w interrupt 0 */
#define CR_HINT1	0x00000800	/* h/w interrupt 1 */
#define CR_HINT2	0x00001000	/* h/w interrupt 2 */
#define CR_HINT3	0x00002000	/* h/w interrupt 3 */
#define CR_HINT4	0x00004000	/* h/w interrupt 4 */
#define CR_HINT5	0x00008000	/* h/w interrupt 5 */

/* alternative interrupt pending bit naming */
#define CR_IP0		0x00000100
#define CR_IP1		0x00000200
#define CR_IP2		0x00000400
#define CR_IP3		0x00000800
#define CR_IP4		0x00001000
#define CR_IP5		0x00002000
#define CR_IP6		0x00004000
#define CR_IP7		0x00008000

#define CR_IMASK	0x0000ff00 	/* interrupt pending mask */
#define CR_XMASK	0x0000007c 	/* exception code mask */
#define CR_XCPT(x)	((x)<<2)


/*
 * R5000 Status Register 
 */
#define SR_IE		0x00000001 	/* interrupt enable */
#define SR_EXL		0x00000002	/* exception level */
#define SR_ERL		0x00000004	/* error level */

#define SR_KSU_MASK	0x00000018	/* ksu mode mask */
#define SR_KSU_USER	0x00000010	/* user mode */
#define SR_KSU_SPVS	0x00000008	/* supervisor mode */
#define SR_KSU_KERN	0x00000000	/* kernel mode */

#define SR_UX		0x00000020	/* mips3 & xtlb in user mode */
#define SR_SX		0x00000040	/* mips3 & xtlb in supervisor mode */
#define SR_KX		0x00000080	/* xtlb in kernel mode */

/* interrupt mask bits */
#define SR_SINT0	0x00000100	/* enable s/w interrupt 0 */
#define SR_SINT1	0x00000200	/* enable s/w interrupt 1 */
#define SR_HINT0	0x00000400	/* enable h/w interrupt 1 */
#define SR_HINT1	0x00000800	/* enable h/w interrupt 2 */
#define SR_HINT2	0x00001000	/* enable h/w interrupt 3 */
#define SR_HINT3	0x00002000	/* enable h/w interrupt 4 */
#define SR_HINT4	0x00004000	/* enable h/w interrupt 5 */
#define SR_HINT5	0x00008000	/* enable h/w interrupt 6 */

/* alternative interrupt mask naming */
#define SR_IM0		0x00000100
#define SR_IM1		0x00000200
#define SR_IM2		0x00000400
#define SR_IM3		0x00000800
#define SR_IM4		0x00001000
#define SR_IM5		0x00002000
#define SR_IM6		0x00004000
#define SR_IM7		0x00008000

#define SR_IMASK	0x0000ff00

/* diagnostic field */
#define SR_DE		0x00010000	/* disable cache/ecc errors */
#define SR_CE		0x00020000	/* use ecc register */
#define SR_CH		0x00040000 	/* cache hit indicator */
#define SR_SR		0x00100000	/* soft reset occurred */
#define SR_TS		0x00200000	/* TLB shutdown */
#define SR_BEV		0x00400000	/* boot exception vectors */
#if #cpu(rm52xx)
#define SR_IL		0x00800000	/* icache lock */
#define SR_DL		0x01000000	/* dcache lock */
#endif

#define SR_RE		0x02000000	/* reverse endian (user mode) */
#define SR_FR		0x04000000	/* 64-bit fpu registers */
#define SR_RP		0x08000000	/* reduce power */

#define SR_CU0		0x10000000	/* coprocessor 0 enable */
#define SR_CU1		0x20000000	/* coprocessor 1 enable */
#define SR_CU2		0x40000000	/* coprocessor 2 enable */
#define SR_XX		0x80000000	/* Mips IV ISA enable */


/*
 * R5000 Config Register 
 */
#define CFG_ECMASK	0x70000000	/* System Clock Ratio */
#define CFG_ECSHIFT	28
#define CFG_EPMASK	0x0f000000	/* Transmit data pattern */
#define CFG_EPD		0x00000000	/* D */
#define CFG_EPDDX	0x01000000	/* DDX */
#define CFG_EPDDXX	0x02000000	/* DDXX */
#define CFG_EPDXDX	0x03000000	/* DXDX */
#define CFG_EPDDXXX	0x04000000	/* DDXXX */
#define CFG_EPDDXXXX	0x05000000	/* DDXXXX */
#define CFG_EPDXXDXX	0x06000000	/* DXXDXX */
#define CFG_EPDDXXXXX	0x07000000	/* DDXXXXX */
#define CFG_EPDXXXDXXX	0x08000000	/* DXXXDXXX */
#define CFG_SSMASK	0x00300000	/* Secondary cache Size */
#define CFG_SSSHIFT	20
#define CFG_SS_512KB	0x00000000
#define CFG_SS_1MB	0x00100000
#define CFG_SS_2MB	0x00200000
#define CFG_SS_NONE	0x00300000
#define CFG_SC		0x00020000	/* Secondary cache absent */
#define CFG_BE		0x00008000	/* Big Endian */
#define CFG_SE		0x00001000	/* Secondary cache Enable */
#define CFG_ICMASK	0x00000e00	/* Instruction cache size */
#define CFG_ICSHIFT	9
#define CFG_DCMASK	0x000001c0	/* Data cache size */
#define CFG_DCSHIFT	6
#define CFG_IB		0x00000020	/* Instruction cache block size */
#define CFG_DB		0x00000010	/* Data cache block size */
#define CFG_K0MASK	0x00000007	/* KSEG0 coherency algorithm */

/*
 * Primary cache mode
 */
#define CFG_C_WTHRU_NOALLOC	0
#define CFG_C_WTHRU_ALLOC	1
#define CFG_C_UNCACHED		2
#define CFG_C_NONCOHERENT	3
#define CFG_C_WBACK		3

/* 
 * Primary Cache TagLo 
 */
#define TAG_PTAG_MASK           0xffffff00      /* Primary Tag */
#define TAG_PTAG_SHIFT          8
#define TAG_PSTATE_MASK         0x000000c0      /* Primary Cache State */
#define TAG_PSTATE_SHIFT        6
#define TAG_ICDEC_MASK		0x0000003c
#define TAG_ICDEC_SHIFT		2
#define TAG_FIFO_MASK         	0x00000002      /* Primary Tag Parity */
#define TAG_FIFO_SHIFT		1
#define TAG_PARITY_MASK         0x00000001      /* Primary Tag Parity */
#define TAG_PARITY_SHIFT        0

#define PSTATE_INVAL		0
#define PSTATE_VALID		3


/* 
 * Secondary Cache TagLo
 * (write through, so no dirty bits)
 */
#define TAG_STAG_MASK           0xffff8000      /* Secondary Tag */
#define TAG_STAG_SHIFT          15
#define TAG_SSTATE_MASK         0x00001c00      /* Secondary Cache State */
#define TAG_SSTATE_SHIFT        10
#define TAG_VINDEX_MASK         0x00000380      /* Secondary Cache VIndex */
#define TAG_VINDEX_SHIFT        7
#define TAG_STAG_SIZE		19		/* Secondary Tag Width */

#define SSTATE_INVAL		0
#define SSTATE_VALID		4

/*
 * CacheErr register
 */
#define CACHEERR_ER		0x80000000
#define CACHEERR_TYPE		0x80000000	/* reference type: 
						   0=Instr, 1=Data */
#define CACHEERR_EC		0x40000000
#define CACHEERR_LEVEL		0x40000000	/* cache level:
						   0=Primary, 1=Secondary */

#define CACHEERR_ED		0x20000000
#define CACHEERR_DATA		0x20000000	/* data field:
						   0=No error, 1=Error */

#define CACHEERR_ET		0x10000000
#define CACHEERR_TAG		0x10000000	/* tag field:
						   0=No error, 1=Error */
#define CACHEERR_ES		0x08000000
#define CACHEERR_REQ		0x08000000	/* request type:
						   0=Internal, 1=External */
#define CACHEERR_EE		0x04000000
#define CACHEERR_BUS		0x04000000	/* error on bus:
						   0=No, 1=Yes */
#define CACHEERR_EB		0x02000000
#define CACHEERR_BOTH		0x02000000	/* Data & Instruction error:
						   0=No, 1=Yes */
#define CACHEERR_SIDX_MASK	0x003ffff8	/* PADDR(21..3) */
#define CACHEERR_SIDX_SHIFT		 3
#define CACHEERR_PIDX_MASK	0x00000007	/* VADDR(14..12) */
#define CACHEERR_PIDX_SHIFT		12

/*
 * Cache operations
 */
#define Index_Invalidate_I               0x0         /* 0       0 */
#define Index_Writeback_Inv_D            0x1         /* 0       1 */
#define Flash_Invalidate_S		 0x3         /* 0       3 */
#define Index_Load_Tag_I                 0x4         /* 1       0 */
#define Index_Load_Tag_D                 0x5         /* 1       1 */
#define Index_Load_Tag_S                 0x7         /* 1       3 */
#define Index_Store_Tag_I                0x8         /* 2       0 */
#define Index_Store_Tag_D                0x9         /* 2       1 */
#define Index_Store_Tag_S                0xB         /* 2       3 */
#define Create_Dirty_Exc_D               0xD         /* 3       1 */
#define Hit_Invalidate_I                 0x10        /* 4       0 */
#define Hit_Invalidate_D                 0x11        /* 4       1 */
#define Fill_I                           0x14        /* 5       0 */
#define Hit_Writeback_Inv_D              0x15        /* 5       1 */
#define Page_Invalidate_S                0x17        /* 5       3 */
#define Hit_Writeback_I                  0x18        /* 6       0 */
#define Hit_Writeback_D                  0x19        /* 6       1 */


/* R5000 EntryHi bits */
#define TLBHI_VPN2MASK	0xffffe000
#define TLBHI_VPN2SHIFT	13
#define TLBHI_VPNMASK	0xfffff000
#define TLBHI_VPNSHIFT	12
#define TLBHI_PIDMASK	0x000000ff
#define TLBHI_PIDSHIFT	0x00000000


/* R5000 EntryLo bits */
#define TLB_PFNMASK	0x3fffffc0
#define TLB_PFNSHIFT	6
#define TLB_FLAGS	0x0000003f
#define TLB_CMASK	0x00000038
#define TLB_CSHIFT	3
#define TLB_D		0x00000004
#define TLB_V		0x00000002
#define TLB_G		0x00000001

#define TLB_WTHRU_NOALLOC	(CFG_C_WTHRU_NOALLOC	<< TLB_CSHIFT)
#define TLB_WTHRU_ALLOC		(CFG_C_WTHRU_ALLOC	<< TLB_CSHIFT)
#define TLB_UNCACHED		(CFG_C_UNCACHED		<< TLB_CSHIFT)
#define TLB_NONCOHERENT		(CFG_C_NONCOHERENT	<< TLB_CSHIFT)
#define TLB_WBACK		(CFG_C_WBACK		<< TLB_CSHIFT)

/* R5000 Index bits */
#define TLBIDX_MASK	0x3f
#define TLBIDX_SHIFT	0

/* R5000 Random bits */
#define TLBRAND_MASK	0x3f
#define TLBRAND_SHIFT	0

#define NTLBID		48	/* total number of tlb entry pairs */

/* macros to constuct tlbhi and tlblo */
#define mktlbhi(vpn,id)   ((((tlbhi_t)(vpn)>>1) << TLBHI_VPN2SHIFT) | \
			   ((id) << TLBHI_PIDSHIFT))
#define mktlblo(pn,flags) (((tlblo_t)(pn) << TLB_PFNSHIFT) | (flags))

/* and destruct them */
#define tlbhiVpn(hi)	((hi) >> TLBHI_VPNSHIFT)
#define tlbhiId(hi)	((hi) & TLBHI_PIDMASK)
#define tlbloPn(lo)	((lo) >> TLB_PFNSHIFT)
#define tlbloFlags(lo)	((lo) & TLB_FLAGS)

#if #cpu(rm52xx)
/* Watchpoint Register (not on all variants) */
#define WATCHLO_PA	0xfffffff8
#define WATCHLO_R	0x00000002
#define WATCHLO_W	0x00000001
#endif

#ifdef __ASSEMBLER__
/* 
 * R5000 Coprocessor 0 register numbers 
 */
#define C0_INDEX	$0
#define C0_INX		$0
#define C0_RANDOM	$1
#define C0_RAND		$1
#define C0_ENTRYLO0	$2
#define C0_TLBLO0	$2
#define C0_ENTRYLO1	$3
#define C0_TLBLO1	$3
#define C0_CONTEXT	$4
#define C0_CTXT		$4
#define C0_PAGEMASK	$5
#define C0_WIRED	$6
#define C0_BADVADDR 	$8
#define C0_VADDR 	$8
#define C0_COUNT 	$9
#define C0_ENTRYHI	$10
#define C0_TLBHI	$10
#define C0_COMPARE	$11
#define C0_STATUS	$12
#define C0_SR		$12
#define C0_CAUSE	$13
#define C0_CR		$13
#define C0_EPC 		$14
#define C0_PRID		$15
#define C0_CONFIG	$16
#define C0_LLADDR	$17
#if #cpu(rm52xx)
#define C0_WATCHLO	$18
#define C0_WATCHHI	$19
#endif
#define C0_ECC		$26
#define C0_CACHEERR	$27
#define C0_TAGLO	$28
#define C0_TAGHI	$29
#define C0_ERRPC	$30

$index		=	$0
$random		=	$1
$entrylo0	=	$2
$entrylo1	=	$3
$context	=	$4
$pagemask	=	$5
$wired		=	$6
$vaddr 		=	$8
$count 		=	$9
$entryhi	=	$10
$compare	=	$11
$sr		=	$12
$cr		=	$13
$epc 		=	$14
$prid		=	$15
$config		=	$16
$lladdr		=	$17
#if #cpu(rm52xx)
$watchlo	=	$18
$watchhi	=	$19
#endif
$ecc		=	$26
$cacheerr	=	$27
$taglo		=	$28
$taghi		=	$29
$errpc		=	$30

/* 
 * R5000 virtual memory regions (SDE-MIPS only supports 32-bit addressing)
 */
#define	KSEG0_BASE	0x80000000
#define	KSEG1_BASE	0xa0000000
#define	KSEG2_BASE	0xc0000000
#define	KSEGS_BASE	0xc0000000
#define	KSEG3_BASE	0xe0000000
#define RVEC_BASE	0xbfc00000	/* reset vector base */

#define KUSEG_SIZE	0x80000000
#define KSEG0_SIZE	0x20000000
#define KSEG1_SIZE	0x20000000
#define KSEG2_SIZE	0x40000000
#define KSEGS_SIZE	0x20000000
#define KSEG3_SIZE	0x20000000

/* 
 * Translate a kernel virtual address in KSEG0 or KSEG1 to a real
 * physical address and back.
 */
#define KVA_TO_PA(v) 	((v) & 0x1fffffff)
#define PA_TO_KVA0(pa)	((pa) | 0x80000000)
#define PA_TO_KVA1(pa)	((pa) | 0xa0000000)

/* translate betwwen KSEG0 and KSEG1 virtual addresses */
#define KVA0_TO_KVA1(v)	((v) | 0x20000000)
#define KVA1_TO_KVA0(v)	((v) & ~0x20000000)

#else

/*
 * Standard types
 */
typedef unsigned long		paddr_t;	/* a physical address */
typedef unsigned long		vaddr_t;	/* a virtual address */
typedef unsigned long		reg32_t;	/* a 32-bit register */
typedef unsigned long long	reg64_t;	/* a 64-bit register */
#if __mips >= 3
typedef unsigned long long	reg_t;
#else
typedef unsigned long		reg_t;
#endif
#if __mips64
typedef unsigned long long	tlbhi_t;	/* the tlbhi field */
#else
typedef unsigned long		tlbhi_t;	/* the tlbhi field */
#endif
typedef unsigned long		tlblo_t;	/* the tlblo field */

/* 
 * R5000 Coprocessor 0 register numbers 
 */
#define C0_INDEX	0
#define C0_INX		0
#define C0_RANDOM	1
#define C0_RAND		1
#define C0_ENTRYLO0	2
#define C0_TLBLO0	2
#define C0_ENTRYLO1	3
#define C0_TLBLO1	3
#define C0_CONTEXT	4
#define C0_CTXT		4
#define C0_PAGEMASK	5
#define C0_WIRED	6
#define C0_BADVADDR 	8
#define C0_VADDR 	8
#define C0_COUNT 	9
#define C0_ENTRYHI	10
#define C0_TLBHI	10
#define C0_COMPARE	11
#define C0_STATUS	12
#define C0_SR		12
#define C0_CAUSE	13
#define C0_CR		13
#define C0_EPC 		14
#define C0_PRID		15
#define C0_CONFIG	16
#define C0_LLADDR	17
#if #cpu(rm52xx)
#define C0_WATCHLO	18
#define C0_WATCHHI	19
#endif
#define C0_ECC		26
#define C0_CACHEERR	27
#define C0_TAGLO	28
#define C0_TAGHI	29
#define C0_ERRPC	30

#define r5kc0_nop() \
  __asm__ __volatile ("%(nop%)") 

#define r5k_sync() \
  __asm__ __volatile ("sync")

/* 
 * Define generic macros for accessing the R3000 coprocessor 0 registers.
 * Most apart from "set" return the original register value.
 */

#define r5kc0_get(reg) \
({ \
  register reg32_t __r; \
  __asm__ __volatile ("mfc0 %0,$" #reg : "=r" (__r)); \
  __r; \
})

#define r5kc0_set(reg, val) \
({ \
    register reg32_t __r = (val); \
    __asm__ __volatile ("%(mtc0 %0,$" #reg "; nop; nop; nop%)" : : "r" (__r) : "memory");\
    __r; \
})

#define r5kc0_xch(reg, val) \
({ \
    register reg32_t __o, __n = (val); \
    __asm__ __volatile ("mfc0 %0,$" #reg : "=r" (__o)); \
    __asm__ __volatile ("%(mtc0 %0,$" #reg "; nop; nop; nop%)" : : "r" (__n) : "memory");\
    __o; \
})


#define r5kc0_bis(reg, val) \
({ \
    register reg32_t __o, __n; \
    __asm__ __volatile ("mfc0 %0,$" #reg : "=r" (__o)); \
    __n = __o | (val); \
    __asm__ __volatile ("%(mtc0 %0,$" #reg "; nop; nop; nop%)" : : "r" (__n) : "memory");\
    __o; \
})


#define r5kc0_bic(reg, val) \
({ \
    register reg32_t __o, __n; \
    __asm__ __volatile ("mfc0 %0,$" #reg : "=r" (__o)); \
    __n = __o &~ (val); \
    __asm__ __volatile ("%(mtc0 %0,$" #reg "; nop; nop; nop%)" : : "r" (__n) : "memory");\
    __o; \
})


/* R5000 Status register (NOTE they are NOT atomic operations) */
#define r5k_getsr()		r5kc0_get(12)
#define r5k_setsr(v)		r5kc0_set(12,v)
#define r5k_xchsr(v)		r5kc0_xch(12,v)
#define r5k_bicsr(v)		r5kc0_bic(12,v)
#define r5k_bissr(v)		r5kc0_bis(12,v)

/* R5000 Cause register (NOTE they are NOT atomic operations) */
#define r5k_getcr()		r5kc0_get(13)
#define r5k_setcr(v)		r5kc0_set(13,v)
#define r5k_xchcr(v)		r5kc0_xch(13,v)
#define r5k_biccr(v)		r5kc0_bic(13,v)
#define r5k_biscr(v)		r5kc0_bis(13,v)

/* R5000 Context register */
#define r5k_getcontext()	r5kc0_get(4)
#define r5k_setcontext(v)	r5kc0_set(4,v)
#define r5k_xchcontext(v)	r5kc0_xch(4,v)

/* R5000 EntryHi register */
#define r5k_getentryhi()	r5kc0_get(10)
#define r5k_setentryhi(v)	r5kc0_set(10,v)
#define r5k_xchentryhi(v)	r5kc0_xch(10,v)

/* R5000 PrID register */
#define r5k_getprid()		r5kc0_get(15)

/* R5000 PageMask register */
#define r5k_getpagemask()	r5kc0_get(5)
#define r5k_setpagemask(v)	r5kc0_set(5,v)
#define r5k_xchpagemask(v)	r5kc0_xch(5,v)

/* R5000 Wired register */
#define r5k_getwired()		r5kc0_get(6)
#define r5k_setwired(v)		r5kc0_set(6,v)
#define r5k_xchwired(v)		r5kc0_xch(6,v)

/* R5000 Count register */
#define r5k_getcount()		r5kc0_get(9)
#define r5k_setcount(v)		r5kc0_set(9,v)
#define r5k_xchcount(v)		r5kc0_xch(9,v)

/* R5000 Compare register*/
#define r5k_getcompare()	r5kc0_get(11)
#define r5k_setcompare(v)	r5kc0_set(11,v)
#define r5k_xchcompare(v)	r5kc0_xch(11,v)

/* R5000 Config register */
#define r5k_getconfig()		r5kc0_get(16)
#define r5k_setconfig(v)	r5kc0_set(16,v)
#define r5k_xchconfig(v)	r5kc0_xch(16,v)
#define r5k_bicconfig(v)	r5kc0_bic(16,v)
#define r5k_bisconfig(v)	r5kc0_bis(16,v)

#if #cpu(rm52xx)
/* R5000 WatchLo register */
#define r5k_getwatchlo()	r5kc0_get(18)
#define r5k_setwatchlo(v)	r5kc0_set(18,v)
#define r5k_xchwatchlo(v)	r5kc0_xch(18,v)
#endif

/* R5000 ECC register */
#define r5k_getecc()		r5kc0_get(26)
#define r5k_setecc(x)		r5kc0_set(26, x)

/* R5000 TagLo register */
#define r5k_gettaglo()		r5kc0_get(28)
#define r5k_settaglo(x)		r5kc0_set(28, x)

/* Generic equivalents */
#define mips_getsr()		r5k_getsr()
#define mips_setsr(v)		r5k_setsr(v)
#define mips_xchsr(v)		r5k_xchsr(v)
#define mips_bicsr(v)		r5k_bicsr(v)
#define mips_bissr(v)		r5k_bissr(v)

#define mips_getcr()		r5k_getcr()
#define mips_setcr(v)		r5k_setcr(v)
#define mips_xchcr(v)		r5k_xchcr(v)
#define mips_biccr(v)		r5k_biccr(v)
#define mips_biscr(v)		r5k_biscr(v)

#define mips_getprid()		r5k_getprid()

/*
 * R5000 cache functions
 */
void r5k_hit_writeback_inv_dcache (vaddr_t va, size_t n);


/* 
 * R5000 TLB acccess 
 */
void	r5k_tlbri (tlbhi_t *, tlblo_t *, tlblo_t *, unsigned *, unsigned);
void	r5k_tlbwi (tlbhi_t, tlblo_t, tlblo_t, unsigned, unsigned);
void	r5k_tlbwr (tlbhi_t, tlblo_t, tlblo_t, unsigned);
int	r5k_tlbrwr (tlbhi_t, tlblo_t, tlblo_t, unsigned);
int	r5k_tlbprobe (tlbhi_t, tlblo_t *, tlblo_t *, unsigned *);
void	r5k_tlbinval (tlbhi_t);
void	r5k_tlbinvalall (void);

/* 
 * R5000 virtual memory regions (SDE-MIPS only supports 32-bit addressing)
 */
#define KUSEG_BASE 	((void *)0x00000000)
#define KSEG0_BASE	((void *)0x80000000)
#define KSEG1_BASE	((void *)0xa0000000)
#define KSEG2_BASE	((void *)0xc0000000)
#define KSEGS_BASE	((void *)0xc0000000)
#define KSEG3_BASE	((void *)0xe0000000)
#define RVEC_BASE	((void *)0xbfc00000)	/* reset vector base */

#define KUSEG_SIZE	0x80000000u
#define KSEG0_SIZE	0x20000000u
#define KSEG1_SIZE	0x20000000u
#define KSEG2_SIZE	0x40000000u
#define KSEGS_SIZE	0x20000000u
#define KSEG3_SIZE	0x20000000u

/* 
 * Translate a kernel virtual address in KSEG0 or KSEG1 to a real
 * physical address and back.
 */
#define KVA_TO_PA(v) 	((paddr_t)(v) & 0x1fffffff)
#define PA_TO_KVA0(pa)	((void *) ((pa) | 0x80000000))
#define PA_TO_KVA1(pa)	((void *) ((pa) | 0xa0000000))

/* translate betwwen KSEG0 and KSEG1 virtual addresses */
#define KVA0_TO_KVA1(v)	((void *) ((unsigned)(v) | 0x20000000))
#define KVA1_TO_KVA0(v)	((void *) ((unsigned)(v) & ~0x20000000))

/* Test for KSEGS */
#define IS_KVA(v)	((int)(v) < 0)
#define IS_KVA0(v)	(((unsigned)(v) >> 29) == 0x4)
#define IS_KVA1(v)	(((unsigned)(v) >> 29) == 0x5)
#define IS_KVA01(v)	(((unsigned)(v) >> 30) == 0x2)
#define IS_KVAS(v)	(((unsigned)(v) >> 29) == 0x6)
#define IS_KVA2(v)	(((unsigned)(v) >> 29) == 0x7)
#define IS_UVA(v)	((int)(v) >= 0)

/* convert register type to address and back */
#define VA_TO_REG(v)	((long)(v))		/* sign-extend 32->64 */
#define REG_TO_VA(v)	((void *)(long)(v))	/* truncate 64->32 */

/*
 * R5000 can set the page size on each TLB entry,
 * we present the common case here.
 */
#define VMPGSIZE 	4096
#define VMPGMASK 	(VMPGSIZE-1)
#define VMPGSHIFT 	12

/* virtual address to virtual page number and back */
#define vaToVpn(va)	((reg_t)(va) >> VMPGSHIFT)
#define vpnToVa(vpn)	(void *)((vpn) << VMPGSHIFT)

/* physical address to physical page number and back */
#define paToPn(pa)	((pa) >> VMPGSHIFT)
#define pnToPa(pn)	((paddr_t)((pn) << VMPGSHIFT))

#endif /* !ASSEMBLER */

#ifdef __cplusplus
}
#endif
#endif /* _R5KC0_H_ */
