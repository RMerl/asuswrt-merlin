/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  L2 Cache Diagnostic			File: diag_l2cache.S
    *  
    *  A diagnostic for the L2 cache.  On pass2 parts, this diag
    *  will disable portions of the cache as necessary.
    *  
    *  Author:  Zongbo Chen (zongbo@broadcom.com)
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

#include "sb1250_defs.h"	/* include definitions for MAKEMASK etc. */
#include "diag_l1cache.h"
#include "sb1250_l2c.h"
#include "sb1250_scd.h"
#include "sb1250_regs.h"
#include "sbmips.h"
#include "diag_l2util.h"
#include "mipsmacros.h"
#include "sb1250_genbus.h"
#include "bsp_config.h"

/*#define _SIMULATOR_*/

/*
 * This lets us override the WID by poking values into our PromICE 
 */
#ifdef _MAGICWID_
#undef A_SCD_SYSTEM_REVISION
#define A_SCD_SYSTEM_REVISION 0x1FC00508
#endif

/* We'll always define pass2.  This routine will not be run unless 
   we're on a pass2 cpu */

#ifndef _SB1250_PASS2_
#define _SB1250_PASS2_
#endif

#define C0_DEBUG $23
#define C0_DataLo C0_TAGLO
#define C0_DataHi C0_TAGHI
#define C0_TagLo C0_TAGLO
#define C0_TagHi  C0_TAGHI
 
#define DMTC0s(reg, c0reg, sel) \
        dmtc0   reg, c0reg, sel; \
        ssnop                   ; \
        ssnop
 
#define DMFC0s(reg, c0reg, sel) \
        dmfc0   reg, c0reg, sel; \
        ssnop                   ; \
        ssnop
 
#define MTC0s(reg, c0reg, sel) \
        mtc0   reg, c0reg, sel; \
        ssnop                   ; \
        ssnop
 
#define MFC0s(reg, c0reg, sel) \
        mfc0   reg, c0reg, sel; \
        ssnop                   ; \
        ssnop
 
#define DMFC0(reg, c0reg ) \
        dmfc0   reg, c0reg; \
        ssnop                   ; \
        ssnop
 
#define DMTC0(reg, c0reg) \
        dmtc0   reg, c0reg; \
        ssnop                   ; \
        ssnop

#undef MFC0 			/* avoid redef'n of mipsmacros.h version */
#define MFC0(reg, c0reg ) \
        mfc0   reg, c0reg; \
        ssnop                   ; \
        ssnop
 
#undef MTC0 			/* avoid redef'n of mipsmacros.h version */
#define MTC0(reg, c0reg) \
        mtc0   reg, c0reg; \
        ssnop                   ; \
        ssnop
                                                                                         
#ifdef _SIMULATOR_
#define LINES_TO_TEST 2
#define LOOP_COUNT 0x16
#else
#define TEST_ALL_LINES
#define LOOP_COUNT 0x1600000
#endif

#define START_BLK 0
#define TOTAL_BLKS 16

#define XKPHYS_C_COH	0x8000000000000000
#define XKPHYS_U_ACC	0xb800000000000000
#define XKPHYS_C_COH_EXC 0xa000000000000000
#define XKPHYS_UNC 	0x9000000000000000

#define L2_RAM_BASE_ADDR        0x00D0180000
#define L2M_READ_RAW_ACCESS 0x00200000
#define	L2M_WRITE_LAST_ECC  0x00400000
#define	L2M_WRITE_TAG       0x00200000
#define	L2M_WRITE_TAG_LAST_ECC 0x00600000

#undef PHYS_TO_XKPHYS			/* avoid redef'n of sbmips.h version */
#define PHYS_TO_XKPHYS(x) (0x8000000000000000|(x))
#define PHYS_TO_XKPHYS_UNC(x) (0x9000000000000000|(x))

#define C0_Status               $12
#undef C0_SR				/* avoid redef'n of sbmips.h version */
#define C0_SR                   C0_Status                                                                               
#define C0_Debug 		C0_DEBUG
#define SR_KX              	(1 << 7)

#ifdef LEDS_PHYS
#define USE_LEDS
#endif

#define RUN_FROM_K0

#define LED_CHAR0       (32+8*3)
#define LED_CHAR1       (32+8*2)
#define LED_CHAR2       (32+8*1)
#define LED_CHAR3       (32+8*0)

#define MAC2_HASH0              0x10066240
 
#define SET_LEDS(leds, temp0, temp1) \
        li      temp0, PHYS_TO_K1(LEDS_PHYS); \
        srl     temp1, leds, 24; \
        and     temp1, temp1, 0xFF; \
        sb      temp1, LED_CHAR0(temp0); \
        srl     temp1, leds, 16; \
        and     temp1, temp1, 0xFF; \
        sb      temp1, LED_CHAR1(temp0); \
        srl     temp1, leds, 8; \
        and     temp1, temp1, 0xFF; \
        sb      temp1, LED_CHAR2(temp0); \
        and     temp1, leds, 0xFF; \
        sb      temp1, LED_CHAR3(temp0)


#define SET_LEDS_HI(leds, temp0, temp1) \
        li      temp0, PHYS_TO_XKPHYS_UNC(LEDS_PHYS); \
        srl     temp1, leds, 8; \
        and     temp1, temp1, 0xFF; \
        sb      temp1, LED_CHAR0(temp0); \
        and     temp1, leds, 0xFF; \
        sb      temp1, LED_CHAR1(temp0)

#define SET_LEDS_LO(leds, temp0, temp1) \
        li      temp0, PHYS_TO_XKPHYS_UNC(LEDS_PHYS); \
        srl     temp1, leds, 8; \
        and     temp1, temp1, 0xFF; \
        sb      temp1, LED_CHAR2(temp0); \
        and     temp1, leds, 0xFF; \
        sb      temp1, LED_CHAR3(temp0)

#define DATA_TEST
#define DATA_ECC_TEST


#define LED_CHAR0       (32+8*3)
#define LED_CHAR1       (32+8*2)
#define LED_CHAR2       (32+8*1)
#define LED_CHAR3       (32+8*0)



	.text
	.set mips64


#define DISAB_TOP    0x800
#define DISAB_BOT    0x400
#define DISAB_RGT    0x200
#define DISAB_LFT    0x100
#define FAILURE      0xFFFFFFFF	/* complete failure, cannot continue */


disab_table:

/*
 * This table maps failure modes to "disable" masks.
 * For each combination of failed quadrants, disable half
 * the cache that contains those quadrants.  Note that since
 * we can only disable top OR bottom and left OR right, not
 * all combinations will work and will yield diagnostic failure.
 */

	.word	0			/* Entire cache functional */
	.word	DISAB_LFT		/* BR=Good TR=Good BL=Good TL=BAD  */
	.word	DISAB_LFT		/* BR=Good TR=Good BL=BAD  TL=Good */
	.word	DISAB_LFT		/* BR=Good TR=Good BL=BAD  TL=BAD  */
	.word	DISAB_RGT		/* BR=Good TR=BAD  BL=Good TL=Good */
	.word	DISAB_TOP|DISAB_RGT	/* BR=Good TR=BAD  BL=Good TL=BAD  */
	.word	DISAB_RGT|DISAB_BOT	/* BR=Good TR=BAD  BL=BAD  TL=Good */
	.word	DISAB_LFT|DISAB_TOP	/* BR=Good TR=BAD  BL=BAD  TL=BAD  */
	.word	DISAB_RGT		/* BR=BAD  TR=Good BL=Good TL=Good */
	.word	DISAB_LFT|DISAB_BOT	/* BR=BAD  TR=Good BL=Good TL=BAD  */
	.word	DISAB_BOT|DISAB_RGT	/* BR=BAD  TR=Good BL=BAD  TL=Good */
	.word	DISAB_LFT|DISAB_BOT	/* BR=BAD  TR=Good BL=BAD  TL=BAD  */
	.word	DISAB_RGT		/* BR=BAD  TR=BAD  BL=Good TL=Good */
	.word	DISAB_RGT|DISAB_TOP	/* BR=BAD  TR=BAD  BL=Good TL=BAD  */
	.word	DISAB_RGT|DISAB_BOT	/* BR=BAD  TR=BAD  BL=BAD  TL=Good */
	.word	FAILURE			/* BR=BAD  TR=BAD  BL=BAD  TL=BAD  */

	


LEAF(diag_main)

        move    fp, ra

	/*
	 * Don't do any of this diagnostic unless we're greater than PASS2 SB1250.
	 */

	la	t0,PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
	ld	t0,0(t0)		/* get system revision */

	and	t1,t0,(0xF0FF << S_SYS_PART)   /* ignore CPU count */
	li	t2,V_SYS_PART(0x1050)	       /* test against ignored count */
	bne	t2,t1,2f			/* go if not a sb1x50 */
	 nop

	and	t1,t0,M_SYS_REVISION
	li	t2,V_SYS_REVISION(K_SYS_REVISION_PASS3)
	bge	t1,t2,3f			/* run diags if >= pass3 (0x20) */ 
	 nop
	li	t2,V_SYS_REVISION(K_SYS_REVISION_PASS2)
	bge	t1,t2,1f			/* go if pass2 or better (>= 0x03) */
	 nop
2:	j	ra
	 nop

1:	dsrl	t1,t0,S_SYS_WID			/* Get Wafer ID register */
	bne	t1,zero,2b			/* leave if register is set (no diags) */
	/* Otherwise, run the diagnostic if the wafer ID is not set. */
	/* Special case of A6 parts (revid==4 && wid!=0) will NOT run diags */

3:

#ifdef USE_LEDS
	dli	t8, (('J'<<24)|('U'<<16)|('M'<<8)|'P')
	SET_LEDS(t8, t6, t7)
#endif

#ifdef RUN_FROM_K0
        MFC0    (t2,C0_CONFIG)
        dsrl    t2, t2, 3
        dsll    t2, t2, 3
        or      t2, t2, 1
        MTC0    (t2,C0_CONFIG)
        jal     to_kseg0
        nop                                                                                   
#endif 

#ifdef USE_LEDS
	dli	t8, (('C'<<24)|('A'<<16)|('S'<<8)|'H')
	SET_LEDS(t8, t6, t7) 
#endif

#ifdef RUN_FROM_K0
	li	t0, LOOP_COUNT
1:	addi	t0, t0, -1
	bne	t0, zero, 1b
#endif

/* set up for 64 bit addressing */
	DMFC0	(t2,C0_SR)
	or	t1,t2,SR_KX
	DMTC0	(t1,C0_SR)

        MFC0s  (t2,C0_Debug,2)
        or      t1, t2, 0x3004
        MTC0s  (t1,C0_Debug,2)                                    
 
	/* initialize the cache */
  	jal	sb1250_l2cache_init1   

        /* use mac2 hash0 reg as result reg, init it to 0 */
        li	t0, PHYS_TO_XKPHYS_UNC(MAC2_HASH0)
	move 	t1, zero       
	sd 	t1, 0(t0)
        
        /* init quarter number to 0 */
        move 	k0, $0

next_quarter: 
	move	v0, $0
#ifdef DATA_TEST
	jal	l2dtest_bg_rw_uac
	nop
	or 	t0, v0, v1
	or 	t0, t0, s4
	or 	t0, t0, s5
	bne	t0, $0, test_fail
data_fail:
	/*bne	v0, $0, data_fail*/
	nop

#ifdef RUN_FROM_K0
	li	t0, LOOP_COUNT
1:	addi	t0, t0, -1
	bne	t0, zero, 1b
#endif

#ifdef DATA_ECC_TEST
	jal	l2dtest_data_ecc
	nop
	or 	t0, v0, v1
	or 	t0, t0, s4
	or 	t0, t0, s5
	bne	t0, $0, test_fail
ecc_fail:
	/*bne	v0, $0, ecc_fail*/
	nop
#endif

/*
#else
*/

#ifdef RUN_FROM_K0
	li	t0, LOOP_COUNT
1:	addi	t0, t0, -1
	bne	t0, zero, 1b
#endif

	jal	l2dtest_tag_data
	nop
	or 	t0, v0, v1
	or 	t0, t0, s4
	or 	t0, t0, s5
	bne	t0, $0, test_fail
tag_fail:
	/*bne	v0, $0, tag_fail*/
	nop

#ifdef RUN_FROM_K0
	li	t0, LOOP_COUNT
1:	addi	t0, t0, -1
	bne	t0, zero, 1b
#endif

	jal	l2dtest_tag_ecc
	nop
	or 	t0, v0, v1
	or 	t0, t0, s4
	or 	t0, t0, s5
	bne	t0, $0, test_fail
tagecc_fail:
	/*bne	v0, $0, tagecc_fail*/
	nop
 	j	test_success
        nop
#endif

#ifdef RUN_FROM_K0
	li	t0, LOOP_COUNT
1:	addi	t0, t0, -1
	bne	t0, zero, 1b
        nop
#endif

test_fail:
	addi	t0, zero, 1
	sll	t0, t0, k0
	li	t1, PHYS_TO_XKPHYS_UNC(MAC2_HASH0) 
	ld	t2, 0(t1)
	or 	t0, t0, t2
	sd 	t0, 0(t1)

test_success:
        addi	k0, k0, 1
	slt	t0, k0, 4
	bne	t0, zero, next_quarter 	

    DMFC0	(t2,C0_SR)
    li	t0, SR_KX
    not t0, t0
    and	t1,t2,t0
    DMTC0	(t1,C0_SR)


/*
 * Disable the bad quadrant(s) of the cache
 */

	li	t1,PHYS_TO_K1(MAC2_HASH0)
	ld	t1,0(t1)
	LOADREL(t2,disab_table)
	sll	t1,2			/* make a word offset */
	add	t2,t2,t1		/* into the table */
	lw	t0,(t2)			/* this is the disable mask */
	blt	t0,zero,diag_fail	/* go if total failure */
	 nop

	li	t1,PHYS_TO_K1(A_L2_CACHE_DISABLE)
	or	t1,t1,t0		/* OR in appropriate disable bits */

#ifdef _SB1250_PASS2_
	sd	t0,0(t1)		/* disable the cache */
#else
	bne	t0,zero,diag_fail	/* on pass1, any failure is bad */
	 nop
#endif

        MFC0s  (t2,C0_Debug,2)		/* turn off things we defeatured */
        and      t1, t2, ~0x3004
        MTC0s  (t1,C0_Debug,2)                                    

#ifdef USE_LEDS

	dli	t8, (('D'<<24)|('O'<<16)|('N'<<8)|'E')
	SET_LEDS(t8, t6, t7)
#endif

	move 	ra, fp 
	j	        ra
	 nop        

diag_fail:

	li	t1,PHYS_TO_K1(MAC2_HASH0)
	ld	t1,0(t1)
	add	t1,t1,'0'
	ble	t1,'9',1f
	 nop
	add	t1,t1,('A'-('9'+1))
1:	
	dli	t8, (('L'<<24)|('2'<<16)|(' '<<8)|0)
	or	t8,t8,t1
#ifdef USE_LEDS
	SET_LEDS(t8, t6, t7)
#endif

1:	b	     1b


END(diag_main)			

#ifdef TEST_ALL_LINES
#define L2_LINES_PER_BLOCK 256	
#else
#define L2_LINES_PER_BLOCK LINES_TO_TEST	
#endif

#define R_WRBC_RBLK	s1
#define R_WRBC_LINE	s2
#define R_WRBC_NEXT_WAY s3	
#define R_WRBC_RADDR	t5
#define R_WRBC_SCRATCH0	t6
#define R_WRBC_SCRATCH1	t7
#define R_WRBC_SCRATCH2	s6
#define R_WRBC_SCRATCH3	s7

#define R_PASS_FAIL	t4
	
/********************************************************
 *  l2test_wr_uac_allblocks				*
 *	Write all blocks of the L2 using uncached	*
 *	accelerated writes in mgmt mode.		*
 *	This also marks all blocks as valid and dirty.	*
 *							*
 ********************************************************/
	
#define R_BG_PAT0_0       a0			
#define R_BG_PAT0_1       a1			
#define R_BG_PAT0_2       a2			
#define R_BG_PAT0_3       a3			
 							
#define R_BG_PAT1_0       t0			
#define R_BG_PAT1_1       t1			
/*
#define R_BG_PAT1_2       t2			
#define R_BG_PAT1_3       t3			
*/
#define R_INV_0	          t2			
#define R_INV_1           t3			

#define R_PATTERN_IND   s0
#define R_BG_LINE       s2			
#define R_BG_NEXT_WAY   s3			
#define R_BG_BLOCK      s1		
#define R_BG_WADDR      t5			
#define R_BG_RADDR      t5
			
#define R_BG_SCRATCH0   t6			
#define R_BG_SCRATCH1   t7			
#define R_BG_SCRATCH2   t8			
#define R_BG_SCRATCH3   t9			
#define R_BG_SCRATCH4   s6			
#define R_BG_SCRATCH5   s7			

#define R_ERR_TABLE_0	v0
#define R_ERR_TABLE_1	v1
#define R_ERR_TABLE_2	s4
#define R_ERR_TABLE_3	s5

#define DATA_PATTERN0_0 0
#define DATA_PATTERN0_1 0
#define DATA_PATTERN0_2 0
#define DATA_PATTERN0_3 0
#define PATTERN0_INV0   0
#define PATTERN0_INV1   0

#define DATA_PATTERN1_0 0xffffffffffffffff
#define DATA_PATTERN1_1 0xffffffffffffffff
#define DATA_PATTERN1_2 0xffffffffffffffff
#define DATA_PATTERN1_3 0xffffffffffffffff
#define PATTERN1_INV0   0xffffffffffffffff
#define PATTERN1_INV1   0xffffffffffffffff

#define DATA_PATTERN2_0 0xffffffffffffffff
#define DATA_PATTERN2_1 0xffffffffffffffff
#define DATA_PATTERN2_2 0xffffffffffffffff
#define DATA_PATTERN2_3 0xffffffffffffffff
#define PATTERN2_INV0   0 
#define PATTERN2_INV1   0xffffffffffffffff

#define DATA_PATTERN3_0 0x5555555555555555
#define DATA_PATTERN3_1 0x5555555555555555
#define DATA_PATTERN3_2 0x5555555555555555
#define DATA_PATTERN3_3 0x5555555555555555
#define PATTERN3_INV0   0 
#define PATTERN3_INV1   0

#define DATA_PATTERN4_0 0x3333333333333333
#define DATA_PATTERN4_1 0x3333333333333333
#define DATA_PATTERN4_2 0x3333333333333333
#define DATA_PATTERN4_3 0x3333333333333333
#define PATTERN4_INV0   0 
#define PATTERN4_INV1   0

#define DATA_PATTERN5_0 0x0f0f0f0f0f0f0f0f
#define DATA_PATTERN5_1 0x0f0f0f0f0f0f0f0f
#define DATA_PATTERN5_2 0x0f0f0f0f0f0f0f0f
#define DATA_PATTERN5_3 0x0f0f0f0f0f0f0f0f
#define PATTERN5_INV0   0 
#define PATTERN5_INV1   0

#define DATA_PATTERN6_0 0x00ff00ff00ff00ff
#define DATA_PATTERN6_1 0x00ff00ff00ff00ff
#define DATA_PATTERN6_2 0x00ff00ff00ff00ff
#define DATA_PATTERN6_3 0x00ff00ff00ff00ff
#define PATTERN6_INV0   0 
#define PATTERN6_INV1   0

#define DATA_PATTERN7_0 0x0000ffff0000ffff
#define DATA_PATTERN7_1 0x0000ffff0000ffff
#define DATA_PATTERN7_2 0x0000ffff0000ffff
#define DATA_PATTERN7_3 0x0000ffff0000ffff
#define PATTERN7_INV0   0 
#define PATTERN7_INV1   0

#define DATA_PATTERN8_0 0x00000000ffffffff
#define DATA_PATTERN8_1 0x00000000ffffffff
#define DATA_PATTERN8_2 0x00000000ffffffff
#define DATA_PATTERN8_3 0x00000000ffffffff
#define PATTERN8_INV0   0 
#define PATTERN8_INV1   0

#define DATA_PATTERN9_0 0xffffffffffffffff
#define DATA_PATTERN9_1 0x0000000000000000
#define DATA_PATTERN9_2 0xffffffffffffffff
#define DATA_PATTERN9_3 0x0000000000000000
#define PATTERN9_INV0   0 
#define PATTERN9_INV1   0

#define DATA_PATTERNa_0 0xffffffffffffffff
#define DATA_PATTERNa_1 0xffffffffffffffff
#define DATA_PATTERNa_2 0x0000000000000000
#define DATA_PATTERNa_3 0x0000000000000000
#define PATTERNa_INV0   0 
#define PATTERNa_INV1   0

#undef MAX_PATTERN		/* avoid redef'n of diag_l1cache.h version */
#define MAX_PATTERN     11

#define BG_WRITE_PAT(addr,pattern0,pattern1,pattern2,pattern3,inv0,inv1)		\
	li	R_BG_SCRATCH0, 2;	\
1:	sd	pattern0, 0(addr);	\
	sd	pattern1, 8(addr);	\
	sd      pattern2, 16(addr);	\
	sd	pattern3, 24(addr); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(addr); \
	xor	pattern0, pattern0, inv0; \
	xor	pattern1, pattern1, inv0; \
	xor	pattern2, pattern2, inv0; \
	xor	pattern3, pattern3, inv0; \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	sd	pattern0, 0(addr);	\
	sd	pattern1, 8(addr);	\
	sd      pattern2, 16(addr);	\
	sd	pattern3, 24(addr); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(addr); \
	xor	pattern0, pattern0, inv1; \
	xor	pattern1, pattern1, inv1; \
	xor	pattern2, pattern2, inv1; \
	xor	pattern3, pattern3, inv1; \
	addi 	R_BG_SCRATCH0, R_BG_SCRATCH0, -1; \
	bne	R_BG_SCRATCH0, zero, 1b;   \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	li      R_BG_SCRATCH0, -4*2*65536; \
        dadd	addr, addr, R_BG_SCRATCH0	/* next line */

#define BG_INVAL_L1(addr)		\
	move 	R_BG_SCRATCH0, addr;	\
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(R_BG_SCRATCH0); \
	daddu 	R_BG_SCRATCH0, R_BG_SCRATCH0, R_BG_NEXT_WAY; \
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(R_BG_SCRATCH0); \
	daddu 	R_BG_SCRATCH0, R_BG_SCRATCH0, R_BG_NEXT_WAY; \
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(R_BG_SCRATCH0); \
	daddu 	R_BG_SCRATCH0, R_BG_SCRATCH0, R_BG_NEXT_WAY; \
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(R_BG_SCRATCH0)

#define BG_CHECK_PAT(addr,pattern0,pattern1,pattern2,pattern3,inv0,inv1)		\
	li	R_BG_SCRATCH4, 2;	\
	dli	R_BG_SCRATCH5, L2M_READ_RAW_ACCESS; \
	or 	addr, addr, R_BG_SCRATCH5; \
1:	ld	R_BG_SCRATCH0, 0(addr);	\
	ld	R_BG_SCRATCH1, 8(addr);	\
        sne     R_BG_SCRATCH0, R_BG_SCRATCH0, pattern0;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH0; \
	ld	R_BG_SCRATCH2, 16(addr);\
        sne     R_BG_SCRATCH1, R_BG_SCRATCH1, pattern1;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH1; \
	ld	R_BG_SCRATCH3, 24(addr);\
        sne     R_BG_SCRATCH2, R_BG_SCRATCH2, pattern2;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH2; \
        sne     R_BG_SCRATCH3, R_BG_SCRATCH3, pattern3;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH3; \
	xor	pattern0, pattern0, inv0; \
	xor	pattern1, pattern1, inv0; \
	xor	pattern2, pattern2, inv0; \
	xor	pattern3, pattern3, inv0; \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	ld	R_BG_SCRATCH0, 0(addr);	\
	ld	R_BG_SCRATCH1, 8(addr);	\
        sne     R_BG_SCRATCH0, R_BG_SCRATCH0, pattern0;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH0; \
	ld	R_BG_SCRATCH2, 16(addr);\
        sne     R_BG_SCRATCH1, R_BG_SCRATCH1, pattern1;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH1; \
	ld	R_BG_SCRATCH3, 24(addr);\
        sne     R_BG_SCRATCH2, R_BG_SCRATCH2, pattern2;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH2; \
        sne     R_BG_SCRATCH3, R_BG_SCRATCH3, pattern3;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH3; \
	xor	pattern0, pattern0, inv1; \
	xor	pattern1, pattern1, inv1; \
	xor	pattern2, pattern2, inv1; \
	xor	pattern3, pattern3, inv1; \
	addi 	R_BG_SCRATCH4, R_BG_SCRATCH4, -1; \
	bne	R_BG_SCRATCH4, zero, 1b;   \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	li      R_BG_SCRATCH0, -4*2*65536; \
        dadd	addr, addr, R_BG_SCRATCH0;	\
	nor	R_BG_SCRATCH5, R_BG_SCRATCH5, $0; \
	and 	addr, addr, R_BG_SCRATCH5

#define BG_CHECK_PAT1(addr,pattern0,pattern1,pattern2,pattern3,inv0,inv1)		\
	li	R_BG_SCRATCH4, 2;	\
	dli	R_BG_SCRATCH5, 0x00200000; \
	or 	addr, addr, R_BG_SCRATCH5; \
1:	ld	R_BG_SCRATCH0, 0(addr);	\
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(addr); \
	ld	R_BG_SCRATCH0, 0(addr);	\
	ld	R_BG_SCRATCH1, 8(addr);	\
        sne     R_BG_SCRATCH0, R_BG_SCRATCH0, pattern0;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH0; \
	ld	R_BG_SCRATCH2, 16(addr);\
        sne     R_BG_SCRATCH1, R_BG_SCRATCH1, pattern1;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH1; \
	ld	R_BG_SCRATCH3, 24(addr);\
        sne     R_BG_SCRATCH2, R_BG_SCRATCH2, pattern2;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH2; \
        sne     R_BG_SCRATCH3, R_BG_SCRATCH3, pattern3;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH3; \
	xor	pattern0, pattern0, inv0; \
	xor	pattern1, pattern1, inv0; \
	xor	pattern2, pattern2, inv0; \
	xor	pattern3, pattern3, inv0; \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	ld	R_BG_SCRATCH0, 0(addr);	\
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(addr); \
	ld	R_BG_SCRATCH0, 0(addr);	\
	ld	R_BG_SCRATCH1, 8(addr);	\
        sne     R_BG_SCRATCH0, R_BG_SCRATCH0, pattern0;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH0; \
	ld	R_BG_SCRATCH2, 16(addr);\
        sne     R_BG_SCRATCH1, R_BG_SCRATCH1, pattern1;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH1; \
	ld	R_BG_SCRATCH3, 24(addr);\
        sne     R_BG_SCRATCH2, R_BG_SCRATCH2, pattern2;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH2; \
        sne     R_BG_SCRATCH3, R_BG_SCRATCH3, pattern3;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_BG_SCRATCH3; \
	xor	pattern0, pattern0, inv1; \
	xor	pattern1, pattern1, inv1; \
	xor	pattern2, pattern2, inv1; \
	xor	pattern3, pattern3, inv1; \
	addi 	R_BG_SCRATCH4, R_BG_SCRATCH4, -1; \
	bne	R_BG_SCRATCH4, zero, 1b;   \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	li      R_BG_SCRATCH0, -4*2*65536; \
        dadd	addr, addr, R_BG_SCRATCH0;	\
	nor	R_BG_SCRATCH5, R_BG_SCRATCH5, $0; \
	and 	addr, addr, R_BG_SCRATCH5

#define INV_PAT(pattern0, pattern1, pattern2, pattern3) \
	not     pattern0, pattern0; \
	not     pattern1, pattern1; \
	not     pattern2, pattern2; \
	not     pattern3, pattern3

#define SET_ERR_TABLE(pass_fail, line)	\
	li	R_BG_SCRATCH0, 0x3f;	\
	and 	R_BG_SCRATCH0, R_BG_SCRATCH0, line;	\
	dsll	R_BG_SCRATCH0, pass_fail, R_BG_SCRATCH0;	\
	dsrl	R_BG_SCRATCH1, line, 6;	\
	bne	R_BG_SCRATCH1, zero, 1f;	\
	li	R_BG_SCRATCH2, 1;	\
	or  	R_ERR_TABLE_0, R_ERR_TABLE_0, R_BG_SCRATCH0;	\
	b 	4f;	\
	nop;	\
1:	bne	R_BG_SCRATCH1, R_BG_SCRATCH2, 2f;	\
	li	R_BG_SCRATCH2, 2;	\
	or  	R_ERR_TABLE_1, R_ERR_TABLE_1, R_BG_SCRATCH0;	\
	b 	4f;	\
	nop;	\
2:	bne	R_BG_SCRATCH1, R_BG_SCRATCH2, 3f;	\
	li	R_BG_SCRATCH2, 3;	\
	or  	R_ERR_TABLE_2, R_ERR_TABLE_2, R_BG_SCRATCH0;	\
	b 	4f;	\
	nop;	\
3:	or  	R_ERR_TABLE_3, R_ERR_TABLE_3, R_BG_SCRATCH0;	\
4:	

#ifdef DATA_TEST	
LEAF(l2dtest_bg_rw_uac)
	/*li	R_BG_BLOCK, START_BLK */
        sll     R_BG_BLOCK, k0, 2
	li	R_BG_NEXT_WAY,0x20000
	move    R_ERR_TABLE_0, $0
	move    R_ERR_TABLE_1, $0
	move    R_ERR_TABLE_2, $0
	move    R_ERR_TABLE_3, $0


bg_next_block:
	/* form address */
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_BG_BLOCK, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('D' << 8)
	SET_LEDS_HI(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif

	move	R_PATTERN_IND, $0

bg_next_patt:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_PATTERN_IND, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('P' << 8)
	SET_LEDS_LO(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif

        bne     R_PATTERN_IND, $0, pattern_ne_0
        dli     R_BG_PAT0_0, DATA_PATTERN0_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN0_1
        dli     R_BG_PAT0_2, DATA_PATTERN0_2
        dli     R_BG_PAT0_3, DATA_PATTERN0_3
        dli     R_INV_0, PATTERN0_INV0
        dli     R_INV_1, PATTERN0_INV1
        j       test_0
        nop
pattern_ne_0:
	li      R_BG_SCRATCH0, 1
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_1
        dli     R_BG_PAT0_0, DATA_PATTERN1_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN1_1
        dli     R_BG_PAT0_2, DATA_PATTERN1_2
        dli     R_BG_PAT0_3, DATA_PATTERN1_3
        dli     R_INV_0, PATTERN1_INV0
        dli     R_INV_1, PATTERN1_INV1
        j       test_0
        nop
pattern_ne_1:
	li      R_BG_SCRATCH0, 2
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_2
        dli     R_BG_PAT0_0, DATA_PATTERN2_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN2_1
        dli     R_BG_PAT0_2, DATA_PATTERN2_2
        dli     R_BG_PAT0_3, DATA_PATTERN2_3
        dli     R_INV_0, PATTERN2_INV0
        dli     R_INV_1, PATTERN2_INV1
        j       test_0
        nop
pattern_ne_2:
	li      R_BG_SCRATCH0, 3
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_3
        dli     R_BG_PAT0_0, DATA_PATTERN3_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN3_1
        dli     R_BG_PAT0_2, DATA_PATTERN3_2
        dli     R_BG_PAT0_3, DATA_PATTERN3_3
        dli     R_INV_0, PATTERN3_INV0
        dli     R_INV_1, PATTERN3_INV1
        j       test_0
        nop
pattern_ne_3:
	li      R_BG_SCRATCH0, 4
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_4
        dli     R_BG_PAT0_0, DATA_PATTERN4_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN4_1
        dli     R_BG_PAT0_2, DATA_PATTERN4_2
        dli     R_BG_PAT0_3, DATA_PATTERN4_3
        dli     R_INV_0, PATTERN4_INV0
        dli     R_INV_1, PATTERN4_INV1
        j       test_0
        nop
pattern_ne_4:
	li      R_BG_SCRATCH0, 5
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_5
        dli     R_BG_PAT0_0, DATA_PATTERN5_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN5_1
        dli     R_BG_PAT0_2, DATA_PATTERN5_2
        dli     R_BG_PAT0_3, DATA_PATTERN5_3
        dli     R_INV_0, PATTERN5_INV0
        dli     R_INV_1, PATTERN5_INV1
        j       test_0
        nop
pattern_ne_5:
	li      R_BG_SCRATCH0, 6
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_6
        dli     R_BG_PAT0_0, DATA_PATTERN6_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN6_1
        dli     R_BG_PAT0_2, DATA_PATTERN6_2
        dli     R_BG_PAT0_3, DATA_PATTERN6_3
        dli     R_INV_0, PATTERN6_INV0
        dli     R_INV_1, PATTERN6_INV1
        j       test_0
        nop
pattern_ne_6:
	li      R_BG_SCRATCH0, 7
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_7
        dli     R_BG_PAT0_0, DATA_PATTERN7_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN7_1
        dli     R_BG_PAT0_2, DATA_PATTERN7_2
        dli     R_BG_PAT0_3, DATA_PATTERN7_3
        dli     R_INV_0, PATTERN7_INV0
        dli     R_INV_1, PATTERN7_INV1
        j       test_0
        nop
pattern_ne_7:
	li      R_BG_SCRATCH0, 8
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_8
        dli     R_BG_PAT0_0, DATA_PATTERN8_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN8_1
        dli     R_BG_PAT0_2, DATA_PATTERN8_2
        dli     R_BG_PAT0_3, DATA_PATTERN8_3
        dli     R_INV_0, PATTERN8_INV0
        dli     R_INV_1, PATTERN8_INV1
        j       test_0
        nop
pattern_ne_8:
	li      R_BG_SCRATCH0, 9
        bne     R_PATTERN_IND, R_BG_SCRATCH0, pattern_ne_9
        dli     R_BG_PAT0_0, DATA_PATTERN9_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERN9_1
        dli     R_BG_PAT0_2, DATA_PATTERN9_2
        dli     R_BG_PAT0_3, DATA_PATTERN9_3
        dli     R_INV_0, PATTERN9_INV0
        dli     R_INV_1, PATTERN9_INV1
        j       test_0
        nop
pattern_ne_9:
        dli     R_BG_PAT0_0, DATA_PATTERNa_0     # data_pattern = pattern[0]
        dli     R_BG_PAT0_1, DATA_PATTERNa_1
        dli     R_BG_PAT0_2, DATA_PATTERNa_2
        dli     R_BG_PAT0_3, DATA_PATTERNa_3
        dli     R_INV_0, PATTERNa_INV0
        dli     R_INV_1, PATTERNa_INV1
        j       test_0
        nop
	
test_0:
	dli	R_BG_WADDR, XKPHYS_C_COH_EXC	/* uncached accelerated */
	dli	R_BG_SCRATCH0, L2_RAM_BASE_ADDR	/* mgmt mode */
	or	R_BG_WADDR,R_BG_WADDR,R_BG_SCRATCH0
	sll	R_BG_SCRATCH0, R_BG_BLOCK, L2_BLOCK_SHIFT /* block number */
	or	R_BG_WADDR, R_BG_WADDR, R_BG_SCRATCH0

	.set	noreorder
	move 	R_BG_LINE, zero
bg_init_next_line:
	/*BG_WRITE_PAT0(R_BG_WADDR) */
	BG_WRITE_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
/*   for (line = 0;  line < max_lines_per_block; line++)  */
	.set	noreorder
	li      R_BG_SCRATCH0, (32)
        dadd	R_BG_WADDR, R_BG_WADDR, R_BG_SCRATCH0	/* next line */
        slt     R_BG_SCRATCH0,R_BG_LINE,L2_LINES_PER_BLOCK-1
	bne	R_BG_SCRATCH0,$0, bg_init_next_line
	add	R_BG_LINE,R_BG_LINE,1
	

/* march element 1 */
	dli	R_WRBC_RADDR, XKPHYS_C_COH_EXC
	dli	R_WRBC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH0
	/* form address for block to be read/written */
	sll	R_WRBC_SCRATCH1, R_WRBC_RBLK, L2_BLOCK_SHIFT
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH1
	li	R_WRBC_NEXT_WAY, 0x20000

	/* way = 0; */
	/* for (line = 0;  line < lines_per_block; line++) {*/
	move 	R_BG_LINE, zero
march1_w0:
	move	R_PASS_FAIL, zero
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_WRITE_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1)
	BG_INVAL_L1(R_BG_WADDR) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	SET_ERR_TABLE(R_PASS_FAIL, R_WRBC_LINE)
	daddiu	R_WRBC_RADDR, R_WRBC_RADDR, 0x20 

        slt     R_BG_SCRATCH0,R_BG_LINE,L2_LINES_PER_BLOCK-1
	bne	R_BG_SCRATCH0,$0,march1_w0 
	add	R_BG_LINE,R_BG_LINE,1

/* march element 2 */
	dli	R_WRBC_RADDR, XKPHYS_C_COH_EXC
	dli	R_WRBC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH0
	/* form address for block to be read/written */
	sll	R_WRBC_SCRATCH1, R_WRBC_RBLK, L2_BLOCK_SHIFT
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH1
	li	R_WRBC_NEXT_WAY, 0x20000
	/* way = 0; */
	/* for (line = 0;  line < lines_per_block; line++) {*/
	move	R_BG_LINE,zero
march2_w0:
	move	R_PASS_FAIL, zero
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_WRITE_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_INVAL_L1(R_BG_WADDR) 
	SET_ERR_TABLE(R_PASS_FAIL, R_WRBC_LINE)
	daddiu	R_WRBC_RADDR, R_WRBC_RADDR, 0x20 

        slt     R_BG_SCRATCH0,R_BG_LINE,L2_LINES_PER_BLOCK-1
	bne	R_BG_SCRATCH0,$0,march2_w0 
	add	R_BG_LINE,R_BG_LINE,1

/* march element 3 */
	dli	R_WRBC_RADDR, XKPHYS_C_COH_EXC          
	dli	R_WRBC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH0
	/* form address for block to be read/written */
        addi    R_WRBC_SCRATCH0, R_WRBC_RBLK, 1
#ifdef TEST_ALL_LINES
	sll	R_WRBC_SCRATCH1, R_WRBC_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_WRBC_SCRATCH1, R_WRBC_RBLK, L2_BLOCK_SHIFT
#endif
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_WRBC_RADDR, R_WRBC_RADDR, -32 
#else 
	daddiu  R_WRBC_RADDR, R_WRBC_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li	R_WRBC_NEXT_WAY, 0x20000
	/* way = 0; */
	/* for (line = 0;  line < lines_per_block; line++) {*/
	li 	R_WRBC_LINE,L2_LINES_PER_BLOCK-1
march3_w0:
	move	R_PASS_FAIL, zero
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_WRITE_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_INVAL_L1(R_BG_WADDR) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	SET_ERR_TABLE(R_PASS_FAIL, R_WRBC_LINE)
	daddiu	R_WRBC_RADDR, R_WRBC_RADDR, -32 

	bne	R_WRBC_LINE,$0, march3_w0
	add	R_WRBC_LINE,R_WRBC_LINE,-1

/* march element 4 */
	dli	R_WRBC_RADDR, XKPHYS_C_COH_EXC          
	dli	R_WRBC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH0
	/* form address for block to be read/written */
        addi    R_WRBC_SCRATCH0, R_WRBC_RBLK, 1
#ifdef TEST_ALL_LINES
	sll	R_WRBC_SCRATCH1, R_WRBC_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_WRBC_SCRATCH1, R_WRBC_RBLK, L2_BLOCK_SHIFT
#endif
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_WRBC_RADDR, R_WRBC_RADDR, -32 
#else 
	daddiu  R_WRBC_RADDR, R_WRBC_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li	R_WRBC_NEXT_WAY, 0x20000
	/* way = 0; */
	/* for (line = 0;  line < lines_per_block; line++) {*/
	li 	R_WRBC_LINE,L2_LINES_PER_BLOCK-1
march4_w0:
	move	R_PASS_FAIL, zero
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_WRITE_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	BG_INVAL_L1(R_BG_WADDR) 
	SET_ERR_TABLE(R_PASS_FAIL, R_WRBC_LINE)
	daddiu	R_WRBC_RADDR, R_WRBC_RADDR, -32 

	bne	R_WRBC_LINE,$0, march4_w0
	add	R_WRBC_LINE,R_WRBC_LINE,-1

/*	
	bne	R_BG_PAT0_0, $0, 0f
	nop
	dli     R_BG_PAT0_0, 0x5555555555555555
	move    R_BG_PAT0_1, R_BG_PAT0_0
	move    R_BG_PAT0_2, R_BG_PAT0_0
	move    R_BG_PAT0_3, R_BG_PAT0_0
	b 	bg_next_patt
	nop
0:
	beq 	R_BG_PAT0_2, $0, 3f 
	nop	
	bne     R_BG_PAT0_3, $0, 4f
	nop
	move    R_BG_PAT0_1, R_BG_PAT0_0
	move    R_BG_PAT0_2, R_BG_PAT0_3
	b       bg_next_patt
4:
	dli 	R_BG_SCRATCH2, 0
	move 	R_BG_SCRATCH0, R_BG_PAT0_0
1:
	dsrl    R_BG_SCRATCH0, R_BG_SCRATCH0, 1
	and     R_BG_SCRATCH1, R_BG_SCRATCH0, 1
	addi	R_BG_SCRATCH2, R_BG_SCRATCH2, 1
	bne     R_BG_SCRATCH1, $0, 1b	
	nop

	dsll    R_BG_SCRATCH2, R_BG_SCRATCH2, 1
	bne     R_BG_SCRATCH2, 0x40, 6f
	dli	R_BG_PAT0_0, -1
	move    R_BG_PAT0_1, $0
	move    R_BG_PAT0_2, R_BG_PAT0_0
	move    R_BG_PAT0_3, $0
	b       bg_next_patt	
6:
	dli     R_BG_SCRATCH3, 1
	dsll    R_BG_SCRATCH3, R_BG_SCRATCH3, R_BG_SCRATCH2
	daddiu  R_BG_SCRATCH3, R_BG_SCRATCH3, -1

	dsll    R_BG_SCRATCH2, R_BG_SCRATCH2, 1
	move    R_BG_PAT0_0, R_BG_SCRATCH3
	beq	R_BG_SCRATCH2, 0x40, 5f
2:	
	dsll    R_BG_SCRATCH3, R_BG_SCRATCH3, R_BG_SCRATCH2
	or	R_BG_PAT0_0, R_BG_PAT0_0, R_BG_SCRATCH3 
	bne     R_BG_SCRATCH3, $0, 2b
	nop

5:
	move    R_BG_PAT0_1, R_BG_PAT0_0
	move    R_BG_PAT0_2, R_BG_PAT0_0
	move    R_BG_PAT0_3, R_BG_PAT0_0
	b 	bg_next_patt
	nop

3:
*/
        addu    R_PATTERN_IND, R_PATTERN_IND, 1
        slt     R_BG_SCRATCH0, R_PATTERN_IND, MAX_PATTERN
        bne     R_BG_SCRATCH0, $0, bg_next_patt                                                         

	li	R_BG_SCRATCH0, 2
1:
	dli	R_BG_SCRATCH1, 2 
2:
	daddiu	R_BG_SCRATCH1, R_BG_SCRATCH1, -1
	bne 	R_BG_SCRATCH1, $0, 2b 
	nop
	daddiu	R_BG_SCRATCH0, R_BG_SCRATCH0, -1
	bne 	R_BG_SCRATCH0, $0, 1b
	nop

/* march element 5 */
	dli	R_WRBC_RADDR, XKPHYS_C_COH_EXC
	dli	R_WRBC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH0
	/* form address for block to be read/written */
	sll	R_WRBC_SCRATCH1, R_WRBC_RBLK, L2_BLOCK_SHIFT
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH1
	li	R_WRBC_NEXT_WAY, 0x20000
	move 	R_WRBC_LINE,zero
march5_w0:
	move	R_PASS_FAIL, zero
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_WRITE_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	SET_ERR_TABLE(R_PASS_FAIL, R_WRBC_LINE)
	daddiu	R_WRBC_RADDR, R_WRBC_RADDR, 32 

        slt     R_BG_SCRATCH0,R_BG_LINE,L2_LINES_PER_BLOCK-1
	bne	R_BG_SCRATCH0,$0,march5_w0 
	add	R_BG_LINE,R_BG_LINE,1

	li	R_BG_SCRATCH0, 2
1:
	dli	R_BG_SCRATCH1, 2 
2:
	daddiu	R_BG_SCRATCH1, R_BG_SCRATCH1, -1
	bne 	R_BG_SCRATCH1, $0, 2b 
	nop
	daddiu	R_BG_SCRATCH0, R_BG_SCRATCH0, -1
	bne 	R_BG_SCRATCH0, $0, 1b
	nop

/* march element 6 */
	dli	R_WRBC_RADDR, XKPHYS_C_COH_EXC
	dli	R_WRBC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH0
	/* form address for block to be read/written */
	sll	R_WRBC_SCRATCH1, R_WRBC_RBLK, L2_BLOCK_SHIFT
	or	R_WRBC_RADDR, R_WRBC_RADDR, R_WRBC_SCRATCH1
	li	R_WRBC_NEXT_WAY, 0x20000
	move 	R_WRBC_LINE,zero
march6_w0:
	move	R_PASS_FAIL, zero
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	BG_CHECK_PAT(R_BG_WADDR,R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3,R_INV_0,R_INV_1) 
	INV_PAT(R_BG_PAT0_0,R_BG_PAT0_1,R_BG_PAT0_2,R_BG_PAT0_3)
	SET_ERR_TABLE(R_PASS_FAIL, R_WRBC_LINE)
	daddiu	R_WRBC_RADDR, R_WRBC_RADDR, 32 

        slt     R_BG_SCRATCH0,R_BG_LINE,L2_LINES_PER_BLOCK-1
	bne	R_BG_SCRATCH0,$0,march6_w0 
	add	R_BG_LINE,R_BG_LINE,1

        /* if error, put test block info into s0 and return */
	or 	R_BG_SCRATCH0, R_ERR_TABLE_0, R_ERR_TABLE_1
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_2
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_3
	beq	R_BG_SCRATCH0, zero, 1f
	nop
	move	s0, R_BG_BLOCK
	or	s0, s0, 1 << 8
	jr	ra
	nop
1:

	addu	R_BG_BLOCK,R_BG_BLOCK,1
	/*slt	R_BG_SCRATCH0, R_BG_BLOCK, (L2_BLOCKS_PER_ROW * L2_BLOCK_ROWS)*/
	addi	R_BG_SCRATCH1, k0, 1
	sll     R_BG_SCRATCH1, R_BG_SCRATCH1, 2
	/*slt	R_BG_SCRATCH0, R_BG_BLOCK, (START_BLK + TOTAL_BLKS)*/
	slt	R_BG_SCRATCH0, R_BG_BLOCK, R_BG_SCRATCH1
	bne	R_BG_SCRATCH0,$0, bg_next_block
	nop

	.set	noreorder
	jr	ra
	nop

bg_err:
	li	v0, 1
	jr	ra
	nop

END(l2dtest_bg_rw_uac)
#endif


#define SCD_BW_BASE	   0x0010020880
#define L2_BASE		   0x0010040000
#define L2_READ_TAG	   0x18
#define	L2_ECC_TAG	   0x38
#define BUS_ERR_STATUS	   0x00
#define BUS_L2_ERRORS	   0x40

#define R_ECC_PAT0_0       a0			
#define R_ECC_PAT0_1       a1			
#define R_ECC_PAT0_2       a2			
#define R_ECC_PAT0_3       a3			
 							
#define R_ECC_PAT1_0       t0			
#define R_ECC_PAT1_1       t1			
#define R_ECC_PAT1_2       t2			
#define R_ECC_PAT1_3       t3			

#define R_ECC_PATT_IND	 s0
#define R_ECC_LINE       s2			
#define R_ECC_NEXT_WAY   s3			
#define R_ECC_BLOCK      s1		
#define R_ECC_WADDR      t5			
#define R_ECC_RADDR      t5
			
#define R_ECC_SCRATCH0   t6			
#define R_ECC_SCRATCH1   t7			
#define R_ECC_SCRATCH2   t8			
#define R_ECC_SCRATCH3   t9			
#define R_ECC_SCRATCH4   s6			
#define R_ECC_SCRATCH5   s7			

/*
#define	DATA_FOR_ECC_PATT0 0x0000	
#define	DATA_FOR_ECC_PATT1 0x0020	
#define	DATA_FOR_ECC_PATT2 0x1032	
#define	DATA_FOR_ECC_PATT3 0x0017	
#define	DATA_FOR_ECC_PATT4 0x1822	

#define	DATA_FOR_ECC_INVPATT0 0x0036	
#define	DATA_FOR_ECC_INVPATT1 0x0016	
#define	DATA_FOR_ECC_INVPATT2 0x1004	
#define	DATA_FOR_ECC_INVPATT3 0x0021	
#define	DATA_FOR_ECC_INVPATT4 0x1814	
*/
#define       DATA_FOR_ECC_PATT0 0x0000
#define       DATA_FOR_ECC_PATT1 0x00ba
#define       DATA_FOR_ECC_PATT2 0x0438
#define       DATA_FOR_ECC_PATT3 0x05be
#define       DATA_FOR_ECC_PATT4 0x0099
 
#define       DATA_FOR_ECC_INVPATT0 0x059c
#define       DATA_FOR_ECC_INVPATT1 0x0526
#define       DATA_FOR_ECC_INVPATT2 0x01a4
#define       DATA_FOR_ECC_INVPATT3 0x0022
#define       DATA_FOR_ECC_INVPATT4 0x0505

#define	ECC_WRITE_PAT(addr, pattern0, pattern1, pattern2, pattern3)\
	dli	R_ECC_SCRATCH2, 4; \
	/*dli	R_ECC_SCRATCH0, 0x00400000;*/ \
	/*nor	R_ECC_SCRATCH1, R_ECC_SCRATCH0, $0;*/ \
1:	sd	pattern0, 0(addr); \
	sd	zero, 8(addr); \
	sd	zero, 16(addr); \
	sd	zero, 24(addr); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(addr); \
	/*or 	addr, R_ECC_SCRATCH0, addr;*/ \
	/*sd	pattern0, 0(addr);*/ \
	/*sd	zero, 8(addr);*/ \
	/*sd	zero, 16(addr);*/ \
	/*sd	zero, 24(addr);*/ \
	/*cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(addr);*/ \
	/*and 	addr, addr, R_ECC_SCRATCH1;*/ \
	move 	R_ECC_SCRATCH3, pattern0; 	\
	move 	pattern0, pattern1;	\
	move 	pattern1, pattern2;	\
	move 	pattern2, pattern3;	\
	move 	pattern3, R_ECC_SCRATCH3; 	\
	addi 	R_ECC_SCRATCH2, R_ECC_SCRATCH2, -1; \
	bne	R_ECC_SCRATCH2, zero, 1b;   \
	daddu 	addr, addr, R_BG_NEXT_WAY; \
	li      R_ECC_SCRATCH0, -4*2*65536; \
        dadd	addr, addr, R_ECC_SCRATCH0
		
#define ECC_CHECK_PAT(addr,pattern0,pattern1,pattern2,pattern3,fail)		\
	dli	R_ECC_SCRATCH4, XKPHYS_UNC; \
	dli	R_ECC_SCRATCH5, SCD_BW_BASE; \
	or	R_ECC_SCRATCH4, R_ECC_SCRATCH4, R_ECC_SCRATCH5; \
	li	R_ECC_SCRATCH5, 4;	\
	sd	zero, BUS_L2_ERRORS(R_ECC_SCRATCH4); \
	sync;	\
1:	ld	R_ECC_SCRATCH0, 0(addr);	\
	ld	R_ECC_SCRATCH1, 8(addr);	\
        sne     R_ECC_SCRATCH0, R_ECC_SCRATCH0, pattern0;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH0; \
	ld	R_ECC_SCRATCH2, 16(addr);\
        sne     R_ECC_SCRATCH1, R_ECC_SCRATCH1, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH1; \
	ld	R_ECC_SCRATCH3, 24(addr);\
        sne     R_ECC_SCRATCH2, R_ECC_SCRATCH2, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH2; \
	ld	R_ECC_SCRATCH0, BUS_L2_ERRORS(R_ECC_SCRATCH4); \
        sne     R_ECC_SCRATCH3, R_ECC_SCRATCH3, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH3; \
	li	R_ECC_SCRATCH2, 0xffff;	\
	and	R_ECC_SCRATCH0, R_ECC_SCRATCH2, R_ECC_SCRATCH0;	\
        sne     R_ECC_SCRATCH0, R_ECC_SCRATCH0, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH0; \
	move 	R_ECC_SCRATCH3, pattern0; 	\
	move 	pattern0, pattern1;	\
	move 	pattern1, pattern2;	\
	move 	pattern2, pattern3;	\
	move 	pattern3, R_ECC_SCRATCH3; 	\
	addi 	R_BG_SCRATCH5, R_BG_SCRATCH5, -1; \
	bne	R_BG_SCRATCH5, zero, 1b;   \
	daddu 	addr, addr, R_ECC_NEXT_WAY; \
	li      R_ECC_SCRATCH0, -4*2*65536; \
        dadd	addr, addr, R_ECC_SCRATCH0

#define ECC_CHECK_PAT1(addr,pattern0,pattern1,pattern2,pattern3,fail)		\
	dli	R_ECC_SCRATCH4, XKPHYS_UNC; \
	dli	R_ECC_SCRATCH5, SCD_BW_BASE; \
	or	R_ECC_SCRATCH4, R_ECC_SCRATCH4, R_ECC_SCRATCH5; \
	li	R_ECC_SCRATCH5, 4;	\
1:	sd	zero, BUS_L2_ERRORS(R_ECC_SCRATCH4); \
	ld	R_ECC_SCRATCH0, 0(addr);	\
	cache	L1CACHEOP(L1C_D,L1C_OP_HITINVAL),0(addr); \
	ld	R_ECC_SCRATCH0, 0(addr);	\
	ld	R_ECC_SCRATCH1, 8(addr);	\
        sne     R_ECC_SCRATCH0, R_ECC_SCRATCH0, pattern0;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH0; \
	ld	R_ECC_SCRATCH2, 16(addr);\
        sne     R_ECC_SCRATCH1, R_ECC_SCRATCH1, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH1; \
	ld	R_ECC_SCRATCH3, 24(addr);\
        sne     R_ECC_SCRATCH2, R_ECC_SCRATCH2, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH2; \
	ld	R_ECC_SCRATCH0, BUS_L2_ERRORS(R_ECC_SCRATCH4); \
        sne     R_ECC_SCRATCH3, R_ECC_SCRATCH3, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH3; \
        sne     R_ECC_SCRATCH0, R_ECC_SCRATCH0, zero;     \
        or      R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH0; \
	move 	R_ECC_SCRATCH3, pattern0; 	\
	move 	pattern0, pattern1;	\
	move 	pattern1, pattern2;	\
	move 	pattern2, pattern3;	\
	move 	pattern3, R_ECC_SCRATCH3; 	\
	addi 	R_BG_SCRATCH5, R_BG_SCRATCH5, -1; \
	bne	R_BG_SCRATCH5, zero, 1b;   \
	daddu 	addr, addr, R_ECC_NEXT_WAY; \
	li      R_ECC_SCRATCH0, -4*2*65536; \
        dadd	addr, addr, R_ECC_SCRATCH0

#ifdef DATA_TEST
#ifdef DATA_ECC_TEST 
LEAF(l2dtest_data_ecc)
		
	/*li 	R_ECC_BLOCK, START_BLK*/
	sll	R_ECC_BLOCK, k0, 2
	li	R_ECC_NEXT_WAY,0x20000
ecc_next_block:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_BG_BLOCK, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('E' << 8)
	SET_LEDS_HI(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif
	
	move	R_ECC_PATT_IND, $0
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT0

ecc_next_pattern:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_ECC_PATT_IND, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('P' << 8)
	SET_LEDS_LO(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif

	dli	R_ECC_WADDR, XKPHYS_C_COH_EXC	/* uncached accelerated */
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR	/* mgmt mode */
	or	R_ECC_WADDR,R_ECC_WADDR,R_BG_SCRATCH0
	sll	R_ECC_SCRATCH0, R_ECC_BLOCK, L2_BLOCK_SHIFT /* block number */
	or	R_ECC_WADDR, R_ECC_WADDR, R_ECC_SCRATCH0

	
	.set	noreorder
	move 	R_ECC_LINE, zero
ecc_init_next_line:
	ECC_WRITE_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3)
	li      R_ECC_SCRATCH0, (32)
        dadd	R_ECC_WADDR, R_ECC_WADDR, R_ECC_SCRATCH0	/* next line */

        slt     R_ECC_SCRATCH0,R_ECC_LINE,L2_LINES_PER_BLOCK-1
	bne	R_ECC_SCRATCH0, $0, ecc_init_next_line
	add	R_ECC_LINE, R_ECC_LINE,1
	
	dli	R_ECC_RADDR, XKPHYS_C_COH_EXC
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_ECC_RADDR, R_ECC_RADDR, R_WRBC_SCRATCH0
	sll	R_ECC_SCRATCH1, R_ECC_BLOCK, L2_BLOCK_SHIFT
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH1
	li	R_ECC_NEXT_WAY, 0x20000

	move 	R_ECC_LINE, zero
ecc_march1:
	move	R_PASS_FAIL, zero
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3,ecc_err)
	ECC_WRITE_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3)
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3,ecc_err)
	BG_INVAL_L1(R_BG_WADDR) 
	SET_ERR_TABLE(R_PASS_FAIL, R_ECC_LINE)
	daddiu	R_ECC_RADDR, R_ECC_RADDR, 0x20 

        slt     R_ECC_SCRATCH0,R_ECC_LINE,L2_LINES_PER_BLOCK-1
	bne	R_ECC_SCRATCH0, $0, ecc_march1
	add	R_ECC_LINE, R_ECC_LINE,1
	
	dli	R_ECC_RADDR, XKPHYS_C_COH_EXC
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_ECC_RADDR, R_ECC_RADDR, R_WRBC_SCRATCH0
	sll	R_ECC_SCRATCH1, R_ECC_BLOCK, L2_BLOCK_SHIFT
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH1
	li	R_ECC_NEXT_WAY, 0x20000

	move 	R_ECC_LINE, zero
ecc_march2:
	move	R_PASS_FAIL, zero
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3,ecc_err)
	ECC_WRITE_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3)
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3,ecc_err)
	BG_INVAL_L1(R_BG_WADDR) 
	SET_ERR_TABLE(R_PASS_FAIL, R_ECC_LINE)
	daddiu	R_ECC_RADDR, R_ECC_RADDR, 0x20 

        slt     R_ECC_SCRATCH0,R_ECC_LINE,L2_LINES_PER_BLOCK-1
	bne	R_ECC_SCRATCH0, $0, ecc_march2
	add	R_ECC_LINE, R_ECC_LINE,1
	
	dli	R_ECC_RADDR, XKPHYS_C_COH_EXC
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH0
        addi    R_ECC_SCRATCH0, R_ECC_BLOCK, 1
#ifdef TEST_ALL_LINES
	sll	R_ECC_SCRATCH1, R_ECC_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_ECC_SCRATCH1, R_ECC_BLOCK, L2_BLOCK_SHIFT
#endif
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_ECC_RADDR, R_ECC_RADDR, -32 
#else 
	daddiu  R_ECC_RADDR, R_ECC_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li 	R_ECC_LINE,L2_LINES_PER_BLOCK-1
ecc_march3:
	move	R_PASS_FAIL, zero
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3,ecc_err)
	ECC_WRITE_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3)
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3,ecc_err)
	BG_INVAL_L1(R_BG_WADDR) 
	SET_ERR_TABLE(R_PASS_FAIL, R_ECC_LINE)
	daddiu	R_ECC_RADDR, R_ECC_RADDR, -32 

	bne	R_ECC_LINE,$0, ecc_march3
	add	R_ECC_LINE,R_ECC_LINE,-1

	dli	R_ECC_RADDR, XKPHYS_C_COH_EXC
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH0
        addi    R_ECC_SCRATCH0, R_ECC_BLOCK, 1
#ifdef TEST_ALL_LINES
	sll	R_ECC_SCRATCH1, R_ECC_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_ECC_SCRATCH1, R_ECC_BLOCK, L2_BLOCK_SHIFT
#endif
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_ECC_RADDR, R_ECC_RADDR, -32 
#else 
	daddiu  R_ECC_RADDR, R_ECC_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li 	R_ECC_LINE,L2_LINES_PER_BLOCK-1
ecc_march4:
	move	R_PASS_FAIL, zero
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3,ecc_err)
	ECC_WRITE_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3)
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3,ecc_err)
	BG_INVAL_L1(R_BG_WADDR) 
	SET_ERR_TABLE(R_PASS_FAIL, R_ECC_LINE)
	daddiu	R_ECC_RADDR, R_ECC_RADDR, -32 

	bne	R_ECC_LINE,$0, ecc_march4
	add	R_ECC_LINE,R_ECC_LINE,-1

	addi	R_ECC_PATT_IND, R_ECC_PATT_IND, 1
	li 	R_ECC_SCRATCH0, 1
	bne	R_ECC_SCRATCH0, R_ECC_PATT_IND, 2f
	nop
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT0
	b	ecc_next_pattern
2:	
	li 	R_ECC_SCRATCH0, 2
	bne	R_ECC_SCRATCH0, R_ECC_PATT_IND, 3f
	nop
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_PATT0
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_INVPATT0
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT0
	b	ecc_next_pattern
	nop
3:	
	li 	R_ECC_SCRATCH0, 3
	bne	R_ECC_SCRATCH0, R_ECC_PATT_IND, 4f
	nop
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_PATT1
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_PATT1
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_PATT1
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT1
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_INVPATT1
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_INVPATT1
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_INVPATT1
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT1
	b	ecc_next_pattern
	nop
4:	
	li 	R_ECC_SCRATCH0, 4
	bne	R_ECC_SCRATCH0, R_ECC_PATT_IND, 5f
	nop
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_PATT2
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_PATT2
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_PATT2
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT2
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_INVPATT2
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_INVPATT2
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_INVPATT2
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT2
	b	ecc_next_pattern
	nop
5:		
	li 	R_ECC_SCRATCH0, 5
	bne	R_ECC_SCRATCH0, R_ECC_PATT_IND, 6f
	nop
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_PATT3
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_PATT3
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_PATT3
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT3
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_INVPATT3
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_INVPATT3
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_INVPATT3
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT3
	b	ecc_next_pattern
	nop
6:		
	li 	R_ECC_SCRATCH0, 6
	bne	R_ECC_SCRATCH0, R_ECC_PATT_IND, 7f
	nop
	dli 	R_ECC_PAT0_0, DATA_FOR_ECC_PATT4
	dli 	R_ECC_PAT0_1, DATA_FOR_ECC_PATT4
	dli 	R_ECC_PAT0_2, DATA_FOR_ECC_PATT4
	dli 	R_ECC_PAT0_3, DATA_FOR_ECC_PATT4
	dli 	R_ECC_PAT1_0, DATA_FOR_ECC_INVPATT4
	dli 	R_ECC_PAT1_1, DATA_FOR_ECC_INVPATT4
	dli 	R_ECC_PAT1_2, DATA_FOR_ECC_INVPATT4
	dli 	R_ECC_PAT1_3, DATA_FOR_ECC_INVPATT4
	b	ecc_next_pattern
	nop
7:		

	li	R_BG_SCRATCH0, 2
1:
	dli	R_BG_SCRATCH1, 2 
2:
	daddiu	R_BG_SCRATCH1, R_BG_SCRATCH1, -1
	bne 	R_BG_SCRATCH1, $0, 2b 
	nop
	daddiu	R_BG_SCRATCH0, R_BG_SCRATCH0, -1
	bne 	R_BG_SCRATCH0, $0, 1b
	nop

	dli	R_ECC_RADDR, XKPHYS_C_COH_EXC
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_ECC_RADDR, R_ECC_RADDR, R_WRBC_SCRATCH0
	sll	R_ECC_SCRATCH1, R_ECC_BLOCK, L2_BLOCK_SHIFT
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH1
	li	R_ECC_NEXT_WAY, 0x20000

	move 	R_ECC_LINE,zero
ecc_march5:
	move	R_PASS_FAIL, zero
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT0_0,R_ECC_PAT0_1,R_ECC_PAT0_2,R_ECC_PAT0_3,ecc_err)
	ECC_WRITE_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3)
	SET_ERR_TABLE(R_PASS_FAIL, R_ECC_LINE)
	daddiu	R_ECC_RADDR, R_ECC_RADDR, 0x20 

        slt     R_ECC_SCRATCH0,R_ECC_LINE,L2_LINES_PER_BLOCK-1
	bne	R_ECC_SCRATCH0, $0, ecc_march5
	add	R_ECC_LINE, R_ECC_LINE,1
	
	dli	R_ECC_RADDR, XKPHYS_C_COH_EXC
	dli	R_ECC_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_ECC_RADDR, R_ECC_RADDR, R_WRBC_SCRATCH0
	sll	R_ECC_SCRATCH1, R_ECC_BLOCK, L2_BLOCK_SHIFT
	or	R_ECC_RADDR, R_ECC_RADDR, R_ECC_SCRATCH1
	li	R_ECC_NEXT_WAY, 0x20000

	li	R_BG_SCRATCH0, 2
1:
	dli	R_BG_SCRATCH1, 2 
2:
	daddiu	R_BG_SCRATCH1, R_BG_SCRATCH1, -1
	bne 	R_BG_SCRATCH1, $0, 2b 
	nop
	daddiu	R_BG_SCRATCH0, R_BG_SCRATCH0, -1
	bne 	R_BG_SCRATCH0, $0, 1b
	nop

	move 	R_ECC_LINE,zero
ecc_march6:
	move	R_PASS_FAIL, zero
	ECC_CHECK_PAT(R_ECC_WADDR,R_ECC_PAT1_0,R_ECC_PAT1_1,R_ECC_PAT1_2,R_ECC_PAT1_3,ecc_err)
	SET_ERR_TABLE(R_PASS_FAIL, R_ECC_LINE)
	daddiu	R_ECC_RADDR, R_ECC_RADDR, 0x20 

        slt     R_ECC_SCRATCH0,R_ECC_LINE,L2_LINES_PER_BLOCK-1
	bne	R_ECC_SCRATCH0, $0, ecc_march6
	add	R_ECC_LINE, R_ECC_LINE,1
	
        /* if error, put test block info into s0 and return */
	or 	R_BG_SCRATCH0, R_ERR_TABLE_0, R_ERR_TABLE_1
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_2
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_3
	beq	R_BG_SCRATCH0, zero, 1f
	nop
	move	s0, R_BG_BLOCK
	or	s0, s0, 2 << 8
	jr	ra
	nop
1:

	addi	R_ECC_BLOCK, R_ECC_BLOCK, 1
	/*slt	R_ECC_SCRATCH0, R_ECC_BLOCK, (L2_BLOCKS_PER_ROW * L2_BLOCK_ROWS)*/
	addi	R_ECC_SCRATCH1, k0, 1
	sll     R_ECC_SCRATCH1, R_ECC_SCRATCH1, 2
	slt	R_ECC_SCRATCH0, R_ECC_BLOCK, R_ECC_SCRATCH1
	/*slt	R_ECC_SCRATCH0, R_ECC_BLOCK, (START_BLK + TOTAL_BLKS)*/
	bne	R_ECC_SCRATCH0,$0, ecc_next_block
	nop

	.set	noreorder
	move	v0, v0 
	jr	ra
	nop

ecc_err:
	li	v0, 1
	jr	ra
	nop

END(l2dtest_data_ecc)
#endif
#endif


#ifdef DATA_TEST

#define L2_TAG_LINES       256
#define	L2_TAG_BLOCKS	   8

#define R_PAT0_0       a0			
#define R_PAT0_1       a1			
#define R_PAT0_2       a2			
#define R_PAT0_3       a3			
 							
#define R_PAT1_0       t0			
#define R_PAT1_1       t1			
#define R_PAT1_2       t2			
#define R_PAT1_3       t3			

#define R_PATT_IND	 s0
#define R_LINE       s2			
#define R_NEXT_WAY   s3			
#define R_BLOCK      s1		
#define R_WADDR      t5			
#define R_RADDR      t5
			
#define R_SCRATCH0   t6			
#define R_SCRATCH1   t7			
#define R_SCRATCH2   t8			
#define R_SCRATCH3   t9			
#define R_SCRATCH4   s6			
#define R_SCRATCH5   s7			

#ifndef _SB1250_PASS2_
#define	L2_READ_TAG_MASK	0x300fffffe0000
#else
#define L2_READ_TAG_MASK        0x3007fffff8000
#endif

#ifndef _SB1250_PASS2_

#define	TAG_PATT0 0x0	
#define	TAG_PATT1 0x1555555	
#define	TAG_PATT2 0x1333333	
#define	TAG_PATT3 0x10f0f0f	
#define	TAG_PATT4 0x0ff00ff	
#define	TAG_PATT5 0x0ffff	

#define	TAG_INVPATT0 0x1ffffff	
#define	TAG_INVPATT1 0x0aaaaaa	
#define	TAG_INVPATT2 0x0cccccc	
#define	TAG_INVPATT3 0x0f0f0f0	
#define	TAG_INVPATT4 0x100ff00	
#define	TAG_INVPATT5 0x1ff0000	

#else

#define TAG_PATT0 0x0
#define TAG_PATT1 0x1555555
#define TAG_PATT2 0x3333333
#define TAG_PATT3 0x10f0f0f
#define TAG_PATT4 0x0ff00ff
#define TAG_PATT5 0x0ffff
 
#define TAG_INVPATT0 0x3ffffff
#define TAG_INVPATT1 0x2aaaaaa
#define TAG_INVPATT2 0x0cccccc
#define TAG_INVPATT3 0x0f0f0f0
#define TAG_INVPATT4 0x300ff00
#define TAG_INVPATT5 0x3ff0000

#endif


#define	TAG_WRITE_PAT(addr, pattern0, pattern1)\
	dli	R_SCRATCH0, L2M_WRITE_TAG; \
	or	R_SCRATCH0, addr, R_SCRATCH0; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0); \
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0); \
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0); \
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0)

#ifndef _SB1250_PASS2_
#define TAG_MAKE_PAT(pattern0, pattern1)\
	li	R_SCRATCH4, 0x800000;\
	and	R_SCRATCH2, R_SCRATCH4, pattern0;\
	dsll	R_SCRATCH2, R_SCRATCH2, 26;\
	li	R_SCRATCH4, 0x1000000;\
	and	R_SCRATCH4, R_SCRATCH4, pattern0;\
	dsll	R_SCRATCH4, R_SCRATCH4, 24;\
	or      R_SCRATCH2, R_SCRATCH2, R_SCRATCH4;\
	li	R_SCRATCH4, 0x7fffff;\
	and	R_SCRATCH4, R_SCRATCH4, pattern0;\
	dsll	R_SCRATCH4, R_SCRATCH4, 17;\
	or      R_SCRATCH2, R_SCRATCH2, R_SCRATCH4;\
	dli	R_SCRATCH4, L2_READ_TAG_MASK; \
	and 	pattern1, R_SCRATCH2, R_SCRATCH4; 

#else

#define TAG_MAKE_PAT(pattern0, pattern1)\
        li      R_SCRATCH4, 0x1000000;\
        and     R_SCRATCH2, R_SCRATCH4, pattern0;\
        dsll    R_SCRATCH2, R_SCRATCH2, 25;\
        li      R_SCRATCH4, 0x2000000;\
        and     R_SCRATCH4, R_SCRATCH4, pattern0;\
        dsll    R_SCRATCH4, R_SCRATCH4, 23;\
        or      R_SCRATCH2, R_SCRATCH2, R_SCRATCH4;\
        li      R_SCRATCH4, 0x3fffff;\
        and     R_SCRATCH4, R_SCRATCH4, pattern0;\
        dsll    R_SCRATCH4, R_SCRATCH4, 17;\
        or      R_SCRATCH2, R_SCRATCH2, R_SCRATCH4;\
        li      R_SCRATCH4, 0xc00000;\
        and     R_SCRATCH4, R_SCRATCH4, pattern0;\
        dsrl    R_SCRATCH4, R_SCRATCH4, 7;\
        or      R_SCRATCH2, R_SCRATCH2, R_SCRATCH4;\
        dli     R_SCRATCH4, L2_READ_TAG_MASK; \
        and     pattern1, R_SCRATCH2, R_SCRATCH4; 

#endif

#define TAG_CHECK_PAT(addr, pattern0, pattern1)\
        li	R_SCRATCH2, 4;\
	dli	R_SCRATCH4, XKPHYS_UNC; \
	dli	R_SCRATCH5, L2_BASE; \
	or	R_SCRATCH4, R_SCRATCH4, R_SCRATCH5; \
	dli	R_SCRATCH5, L2_READ_TAG_MASK;\
	dli	R_SCRATCH0, L2M_READ_RAW_ACCESS; \
	or	R_SCRATCH0, addr, R_SCRATCH0; \
1:	ld 	R_SCRATCH1, (R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern0; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	ld 	R_SCRATCH1, 0x2000(R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern1; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	addi	R_SCRATCH2, R_SCRATCH2, -1;	\
        bne	R_SCRATCH2, zero, 1b;	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY

#define TAG_CHECK_PAT1(addr, pattern0, pattern1)\
        li	R_SCRATCH2, 4;\
	dli	R_SCRATCH4, XKPHYS_UNC; \
	dli	R_SCRATCH5, L2_BASE; \
	or	R_SCRATCH4, R_SCRATCH4, R_SCRATCH5; \
	dli	R_SCRATCH5, L2_READ_TAG_MASK;\
	dli	R_SCRATCH0, 0x00200000; \
	or	R_SCRATCH0, addr, R_SCRATCH0; \
1:	ld 	R_SCRATCH1, (R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern0; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	ld 	R_SCRATCH1, 0x2000(R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern1; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	addi	R_SCRATCH2, R_SCRATCH2, -1;	\
        bne	R_SCRATCH2, zero, 1b;	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY

	
LEAF(l2dtest_tag_data)
/*		
	move    R_ERR_TABLE_0, $0
	move    R_ERR_TABLE_1, $0
	move    R_ERR_TABLE_2, $0
	move    R_ERR_TABLE_3, $0
*/
	/*li 	R_BLOCK, START_BLK*/
        sll	R_BLOCK, k0, 1
	li	R_NEXT_WAY,0x20000
tag_next_block:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_BG_BLOCK, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('T' << 8)
	SET_LEDS_HI(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif
	
	move	R_PATT_IND, $0
	dli 	R_PAT0_0, TAG_PATT0
	dli 	R_PAT0_1, TAG_PATT0
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_INVPATT0
	dli 	R_PAT1_1, TAG_INVPATT0
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)

tag_next_pattern:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_PATT_IND, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('P' << 8)
	SET_LEDS_LO(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif

	dli	R_WADDR, XKPHYS_C_COH_EXC	/* uncached accelerated */
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR	/* mgmt mode */
	or	R_WADDR,R_ECC_WADDR,R_BG_SCRATCH0
	sll	R_SCRATCH0, R_BLOCK, L2_BLOCK_ROW_SHIFT /* block number */
	or	R_WADDR, R_WADDR, R_SCRATCH0

	.set	noreorder
	move 	R_LINE, zero
tag_init_next_line:
	TAG_WRITE_PAT(R_WADDR,R_PAT0_0,R_PAT0_1)
	li      R_SCRATCH0, (32)
        dadd	R_WADDR, R_WADDR, R_SCRATCH0	/* next line */
        
	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_init_next_line
	add	R_LINE, R_LINE,1
	
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1
	li	R_NEXT_WAY, 0x20000

	move 	R_LINE, zero
tag_march1:	
	move 	R_PASS_FAIL, zero
	TAG_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3)  
	TAG_WRITE_PAT(R_WADDR,R_PAT1_0,R_PAT1_1)
	TAG_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_march1
	add	R_LINE, R_LINE,1
	
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1

	move 	R_LINE,zero
tag_march2:	
	move 	R_PASS_FAIL, zero
	TAG_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3)  
	TAG_WRITE_PAT(R_WADDR,R_PAT0_0,R_PAT0_1)
	TAG_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_march2
	add	R_LINE, R_LINE,1
	
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
#ifdef TEST_ALL_LINES
        sll     R_SCRATCH0, R_BLOCK, 1
	addi	R_SCRATCH0, R_SCRATCH0, 1
	sll	R_SCRATCH1, R_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
#endif
	or	R_RADDR, R_RADDR, R_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_RADDR, R_RADDR, -32 
#else 
	daddiu  R_RADDR, R_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li 	R_LINE,L2_LINES_PER_BLOCK-1
tag_march3:	
	move 	R_PASS_FAIL, zero
	TAG_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3)  
	TAG_WRITE_PAT(R_WADDR,R_PAT1_0,R_PAT1_1)
	TAG_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, -32 

	bne	R_LINE,$0, tag_march3
	add	R_LINE,R_LINE,-1

	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
#ifdef TEST_ALL_LINES
        sll     R_SCRATCH0, R_BLOCK, 1
	addi	R_SCRATCH0, R_SCRATCH0, 1
	sll	R_SCRATCH1, R_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
#endif
	or	R_RADDR, R_RADDR, R_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_RADDR, R_RADDR, -32 
#else 
	daddiu  R_RADDR, R_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li 	R_LINE,L2_LINES_PER_BLOCK-1
tag_march4:	
	move 	R_PASS_FAIL, zero
	TAG_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3)  
	TAG_WRITE_PAT(R_WADDR,R_PAT0_0,R_PAT0_1)
	TAG_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, -32 

	bne	R_LINE,$0, tag_march4
	add	R_LINE,R_LINE,-1

	addi	R_PATT_IND, R_PATT_IND, 1
	li 	R_SCRATCH0, 1
	bne	R_SCRATCH0, R_PATT_IND, 2f
	nop
	dli 	R_PAT0_0, TAG_INVPATT0
	dli 	R_PAT0_1, TAG_PATT0
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_PATT0
	dli 	R_PAT1_1, TAG_INVPATT0
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)

	b	tag_next_pattern
2:	
	li 	R_SCRATCH0, 2
	bne	R_SCRATCH0, R_PATT_IND, 3f
	nop
	
	dli 	R_PAT0_0, TAG_PATT2
	dli 	R_PAT0_1, TAG_PATT2
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_INVPATT2
	dli 	R_PAT1_1, TAG_INVPATT2
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_next_pattern
3:	
	li 	R_SCRATCH0, 3
	bne	R_SCRATCH0, R_PATT_IND, 4f
	nop
	
	dli 	R_PAT0_0, TAG_PATT3
	dli 	R_PAT0_1, TAG_PATT3
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_INVPATT3
	dli 	R_PAT1_1, TAG_INVPATT3
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_next_pattern
4:		
	li 	R_SCRATCH0, 4
	bne	R_SCRATCH0, R_PATT_IND, 5f
	nop
	
	dli 	R_PAT0_0, TAG_PATT4
	dli 	R_PAT0_1, TAG_PATT4
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_INVPATT4
	dli 	R_PAT1_1, TAG_INVPATT4
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_next_pattern
5:		
	li 	R_SCRATCH0, 5
	bne	R_SCRATCH0, R_PATT_IND, 6f
	nop
	
	dli 	R_PAT0_0, TAG_PATT5
	dli 	R_PAT0_1, TAG_PATT5
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_INVPATT5
	dli 	R_PAT1_1, TAG_INVPATT5
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_next_pattern
	nop
6:	
	li	R_SCRATCH0, 2
1:
	dli	R_SCRATCH1, 2 
2:
	daddiu	R_SCRATCH1, R_SCRATCH1, -1
	bne 	R_SCRATCH1, $0, 2b 
	nop
	daddiu	R_SCRATCH0, R_SCRATCH0, -1
	bne 	R_SCRATCH0, $0, 1b
	nop

	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1
	li	R_NEXT_WAY, 0x20000

	move 	R_LINE,zero
tag_march5:	
	move 	R_PASS_FAIL, zero
	TAG_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3)  
	TAG_WRITE_PAT(R_WADDR,R_PAT1_0,R_PAT1_1)
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_march5 
	add	R_LINE, R_LINE,1
	
	li	R_SCRATCH0, 2
1:
	dli	R_SCRATCH1, 2 
2:
	daddiu	R_SCRATCH1, R_SCRATCH1, -1
	bne 	R_SCRATCH1, $0, 2b 
	nop
	daddiu	R_SCRATCH0, R_SCRATCH0, -1
	bne 	R_SCRATCH0, $0, 1b
	nop

	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1

	move 	R_LINE,zero
tag_march6:	
	move 	R_PASS_FAIL, zero
	TAG_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3)  
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_march6 
	add	R_LINE, R_LINE,1
	
        /* if error, put test block info into s0 and return */
	or 	R_BG_SCRATCH0, R_ERR_TABLE_0, R_ERR_TABLE_1
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_2
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_3
	beq	R_BG_SCRATCH0, zero, 1f
	nop
	move	s0, R_BG_BLOCK
	or	s0, s0, 3 << 8
	jr	ra
	nop
1:

	addi	R_BLOCK, R_BLOCK, 1
	addi 	R_SCRATCH1, k0, 1
	sll	R_SCRATCH1, R_SCRATCH1, 1
	/*slt	R_SCRATCH0, R_BLOCK, START_BLK+TOTAL_BLKS/2*/
	slt	R_SCRATCH0, R_BLOCK, R_SCRATCH1 
	bne	R_SCRATCH0,$0, tag_next_block
	nop
	.set	noreorder
	jr	ra
	nop

tag_err:
	li	v0, 1
	jr	ra
	nop

END(l2dtest_tag_data)

#define	TAG_ECC_PATT0 0x0	
#define	TAG_ECC_PATT1 0x0000003d	
#define	TAG_ECC_PATT2 0x00000009	
#define	TAG_ECC_PATT3 0x0000001d	

#define	TAG_ECC_INVPATT0 0x0000000f	
#define	TAG_ECC_INVPATT1 0x00000032	
#define	TAG_ECC_INVPATT2 0x00000006	
#define	TAG_ECC_INVPATT3 0x00000012	


#define	TAG_ECC_WRITE_PAT(addr, pattern0, pattern1)\
	dli	R_SCRATCH0, L2M_WRITE_TAG; \
	or	R_SCRATCH0, addr, R_SCRATCH0; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sync;	\
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sync;	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sync;	\
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0); \
	sync;	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sync;	\
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0); \
	sync;	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY; \
	sd	pattern0, 0x18(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0(R_SCRATCH0); \
	sync;	\
	sd	pattern1, 0x2018(R_SCRATCH0); \
	cache	L1CACHEOP(L1C_D,L1C_OP_HIT_WB_INVAL),0x2000(R_SCRATCH0);\
	sync	

#define TAG_ECC_CHECK_PAT(addr, pattern0, pattern1)\
        li	R_SCRATCH2, 2;\
	dli	R_SCRATCH5, XKPHYS_UNC; \
	dli	R_SCRATCH3, SCD_BW_BASE; \
	or	R_SCRATCH3, R_SCRATCH3, R_SCRATCH5; \
	sd	zero, BUS_L2_ERRORS(R_SCRATCH3); \
	sync;	\
	dli	R_SCRATCH4, L2_BASE; \
	or	R_SCRATCH4, R_SCRATCH4, R_SCRATCH5; \
	move	R_SCRATCH0, addr;	\
1:	dli     R_SCRATCH5, L2_READ_TAG_MASK;\
	ld 	R_SCRATCH1, (R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern0; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	ld 	R_SCRATCH1, 0x2000(R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern1; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY;\
	ld 	R_SCRATCH1, (R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern0; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	ld 	R_SCRATCH1, 0x2000(R_SCRATCH0); \
	sync; \
	ld 	R_SCRATCH1, L2_READ_TAG(R_SCRATCH4); \
	and	R_SCRATCH1, R_SCRATCH1, R_SCRATCH5; \
	sne	R_SCRATCH1, R_SCRATCH1, pattern1; \
	or	R_PASS_FAIL, R_PASS_FAIL, R_SCRATCH1; 	\
	addi	R_SCRATCH2, R_SCRATCH2, -1;	\
        bne	R_SCRATCH2, zero, 1b;	\
	daddu 	R_SCRATCH0, R_SCRATCH0, R_NEXT_WAY;\
	/*TAG_CHECK_PAT(addr, pattern0, pattern1) ;*/ \
	dli	R_SCRATCH5, XKPHYS_UNC; \
	dli	R_SCRATCH3, SCD_BW_BASE; \
	or	R_SCRATCH3, R_SCRATCH3, R_SCRATCH5; \
	ld	R_SCRATCH5, BUS_L2_ERRORS(R_SCRATCH3); \
	dsrl	R_SCRATCH5, R_SCRATCH5, 16;	\
	sne 	R_SCRATCH5, R_ECC_SCRATCH5, $0;	\
	or	R_PASS_FAIL, R_PASS_FAIL, R_ECC_SCRATCH5
	
LEAF(l2dtest_tag_ecc)
		
	/*li 	R_BLOCK, START_BLK*/
	sll	R_BLOCK, k0, 1
	li	R_NEXT_WAY,0x20000
tag_ecc_next_block:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_BG_BLOCK, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('P' << 8)
	SET_LEDS_HI(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif
	
	move	R_PATT_IND, $0
	dli 	R_PAT0_0, TAG_ECC_PATT0
	dli 	R_PAT0_1, TAG_ECC_PATT0
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_ECC_INVPATT0
	dli 	R_PAT1_1, TAG_ECC_INVPATT0
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)

tag_ecc_next_pattern:
#ifdef USE_LEDS
	addi	R_BG_SCRATCH2, R_PATT_IND, '0'
	or 	R_BG_SCRATCH2, R_BG_SCRATCH2, ('P' << 8)
	SET_LEDS_LO(R_BG_SCRATCH2, R_BG_SCRATCH0, R_BG_SCRATCH1)
#endif

	dli	R_WADDR, XKPHYS_C_COH_EXC	/* uncached accelerated */
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR	/* mgmt mode */
	or	R_WADDR,R_ECC_WADDR,R_BG_SCRATCH0
	sll	R_SCRATCH0, R_BLOCK, L2_BLOCK_ROW_SHIFT /* block number */
	or	R_WADDR, R_WADDR, R_SCRATCH0

	.set	noreorder
	move 	R_LINE, zero
tag_ecc_init_next_line:
	TAG_ECC_WRITE_PAT(R_WADDR,R_PAT0_0,R_PAT0_1)
	li      R_SCRATCH0, (32)
        dadd	R_WADDR, R_WADDR, R_SCRATCH0	/* next line */
	
	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_ecc_init_next_line
	add	R_LINE, R_LINE,1
	
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1
	li	R_NEXT_WAY, 0x20000

	move 	R_LINE,zero
tag_ecc_march1:	
	move 	R_PASS_FAIL, zero
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3)  
	TAG_ECC_WRITE_PAT(R_WADDR,R_PAT1_0,R_PAT1_1)
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_ecc_march1
	add	R_LINE, R_LINE,1

	/* march2 */	
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1

	move 	R_LINE,zero
tag_ecc_march2:	
	move 	R_PASS_FAIL, zero
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3)  
	TAG_ECC_WRITE_PAT(R_WADDR,R_PAT0_0,R_PAT0_1)
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0,  tag_ecc_march2
	add	R_LINE, R_LINE,1
	
 	/* march 3 */
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
#ifdef TEST_ALL_LINES
        sll     R_SCRATCH0, R_BLOCK, 1
	addi	R_SCRATCH0, R_SCRATCH0, 1
	sll	R_SCRATCH1, R_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
#endif
	or	R_RADDR, R_RADDR, R_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_RADDR, R_RADDR, -32 
#else 
	daddiu  R_RADDR, R_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li 	R_LINE,L2_LINES_PER_BLOCK-1
tag_ecc_march3:	
	move 	R_PASS_FAIL, zero
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3)  
	TAG_ECC_WRITE_PAT(R_WADDR,R_PAT1_0,R_PAT1_1)
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, -32 

	bne	R_LINE,$0, tag_ecc_march3
	add	R_LINE,R_LINE,-1

 	/* march 4 */
	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
#ifdef TEST_ALL_LINES
        sll     R_SCRATCH0, R_BLOCK, 1
	addi	R_SCRATCH0, R_SCRATCH0, 1
	sll	R_SCRATCH1, R_SCRATCH0, L2_BLOCK_SHIFT
#else 
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
#endif
	or	R_RADDR, R_RADDR, R_SCRATCH1
#ifdef TEST_ALL_LINES
	daddiu  R_RADDR, R_RADDR, -32 
#else 
	daddiu  R_RADDR, R_RADDR, L2_LINES_PER_BLOCK*32-32
#endif
	li 	R_LINE,L2_LINES_PER_BLOCK-1
tag_ecc_march4:	
	move 	R_PASS_FAIL, zero
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3)  
	TAG_ECC_WRITE_PAT(R_WADDR,R_PAT0_0,R_PAT0_1)
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3) 
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, -32 

	bne	R_LINE,$0, tag_ecc_march4
	add	R_LINE,R_LINE,-1

	addi	R_PATT_IND, R_PATT_IND, 1
	li 	R_SCRATCH0, 1
	bne	R_SCRATCH0, R_PATT_IND, 2f
	nop
	dli 	R_PAT0_0, TAG_ECC_INVPATT0
	dli 	R_PAT0_1, TAG_ECC_PATT0
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_ECC_PATT0
	dli 	R_PAT1_1, TAG_ECC_INVPATT0
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_ecc_next_pattern
2:	
	li 	R_SCRATCH0, 2
	bne	R_SCRATCH0, R_PATT_IND, 3f
	nop
	dli 	R_PAT0_0, TAG_ECC_PATT1
	dli 	R_PAT0_1, TAG_ECC_PATT1
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_ECC_INVPATT1
	dli 	R_PAT1_1, TAG_ECC_INVPATT1
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_ecc_next_pattern
	nop
3:	
	li 	R_SCRATCH0, 3
	bne	R_SCRATCH0, R_PATT_IND, 4f
	nop
	dli 	R_PAT0_0, TAG_ECC_PATT2
	dli 	R_PAT0_1, TAG_ECC_PATT2
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_ECC_INVPATT2
	dli 	R_PAT1_1, TAG_ECC_INVPATT2
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_ecc_next_pattern
	nop
4:		
	li 	R_SCRATCH0, 4
	bne	R_SCRATCH0, R_PATT_IND, 5f
	nop
	dli 	R_PAT0_0, TAG_ECC_PATT3
	dli 	R_PAT0_1, TAG_ECC_PATT3
	TAG_MAKE_PAT(R_PAT0_0, R_PAT0_2)
	TAG_MAKE_PAT(R_PAT0_1, R_PAT0_3)
	dli 	R_PAT1_0, TAG_ECC_INVPATT3
	dli 	R_PAT1_1, TAG_ECC_INVPATT3
	TAG_MAKE_PAT(R_PAT1_0, R_PAT1_2)
	TAG_MAKE_PAT(R_PAT1_1, R_PAT1_3)
	b	tag_ecc_next_pattern
	nop
5:
	li	R_SCRATCH0, 2
1:
	dli	R_SCRATCH1, 2 
2:
	daddiu	R_SCRATCH1, R_SCRATCH1, -1
	bne 	R_SCRATCH1, $0, 2b 
	nop
	daddiu	R_SCRATCH0, R_SCRATCH0, -1
	bne 	R_SCRATCH0, $0, 1b
	nop

	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1
	li	R_NEXT_WAY, 0x20000

	move 	R_LINE,zero
tag_ecc_march5:	
	move 	R_PASS_FAIL, zero
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT0_2,R_PAT0_3)  
	TAG_ECC_WRITE_PAT(R_WADDR,R_PAT1_0,R_PAT1_1)
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0,  tag_ecc_march5
	add	R_LINE, R_LINE,1
	
	li	R_SCRATCH0, 2
1:
	dli	R_SCRATCH1, 2 
2:
	daddiu	R_SCRATCH1, R_SCRATCH1, -1
	bne 	R_SCRATCH1, $0, 2b 
	nop
	daddiu	R_SCRATCH0, R_SCRATCH0, -1
	bne 	R_SCRATCH0, $0, 1b
	nop

	dli	R_RADDR, XKPHYS_C_COH_EXC
	dli	R_SCRATCH0, L2_RAM_BASE_ADDR
	or	R_RADDR, R_RADDR, R_SCRATCH0
	sll	R_SCRATCH1, R_BLOCK, L2_BLOCK_ROW_SHIFT
	or	R_RADDR, R_RADDR, R_SCRATCH1

	move 	R_LINE,zero
tag_ecc_march6:	
	move 	R_PASS_FAIL, zero
	TAG_ECC_CHECK_PAT(R_WADDR,R_PAT1_2,R_PAT1_3)  
	SET_ERR_TABLE(R_PASS_FAIL, R_LINE)
	daddiu	R_RADDR, R_RADDR, 0x20 

	slt     R_SCRATCH0,R_LINE,L2_LINES_PER_BLOCK-1
	bne	R_SCRATCH0, $0, tag_ecc_march6 
	add	R_LINE, R_LINE,1
	
        /* if error, put test block info into s0 and return */
	or 	R_BG_SCRATCH0, R_ERR_TABLE_0, R_ERR_TABLE_1
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_2
	or	R_BG_SCRATCH0, R_BG_SCRATCH0, R_ERR_TABLE_3
	beq	R_BG_SCRATCH0, zero, 1f
	nop
	move	s0, R_BG_BLOCK
	or	s0, s0, 4 << 8
	jr	ra
	nop
1:

	addi	R_BLOCK, R_BLOCK, 1
	addi	R_SCRATCH1, k0, 1
	sll	R_SCRATCH1, R_SCRATCH1, 1
	slt	R_SCRATCH0, R_BLOCK, R_SCRATCH1 
	/*slt	R_SCRATCH0, R_BLOCK, START_BLK+TOTAL_BLKS/2*/
	bne	R_SCRATCH0,$0,  tag_ecc_next_block
	nop

	.set	noreorder
	jr	ra
	nop

tag_ecc_err:
	li	v0, 1
	jr	ra
	nop

END(l2dtest_tag_ecc)
	
#endif

LEAF(sb1250_l2cache_init1)
#define CACHE_LINE_SIZE	  32

	# Save the old status register, and set the KX bit.
/*
		dmfc0	t2,C0_SR
		or	t1,t2,SR_KX
		dmtc0	t1,C0_SR
*/
	# Start the index at the base of the cache management
	# area, but leave the address bit for "Valid" zero.
	# Note that the management tags are at 00_D000_0000,
	# which cannot be expressed with the PHYS_TO_K1 macro,
	# so well need to use a 64-bit address to get to it.

		
#		dli	t0,PHYS_TO_XKPHYS_UNC(L2C_MGMT_TAG_BASE)
		dli	t8,PHYS_TO_XKPHYS_UNC(A_L2C_MGMT_TAG_BASE)
		dli 	t7, START_BLK
3:		dsll	t4, t7, 13
		dadd	t4, t8, t4

	# Loop through each entry and each way 

		/* li	t1,L2_LINES_PER_BLOCK*L2C_NUM_WAYS */
		li	t5, L2C_NUM_WAYS 

	# Write a zero to the cache management register at each
	# address.
2:		move 	t0, t4
		li	t1, L2_LINES_PER_BLOCK 
1:		sd	zero,(t0)
		nop
		daddu	t0,(CACHE_LINE_SIZE) # size of a cache line
		subu	t1,1
		bne	t1,0,1b	
		nop

		dli	t6, 0x20000
		dadd	t4, t4, t6
		subu 	t5, 1
		bne	t5, 0, 2b
		nop
		
		addu	t7, t7, 1
		slt	t0, t7, START_BLK+TOTAL_BLKS
		bne	t0, zero, 3b
		nop	

	#
	# Restore old KX bit setting
	#
/*
		dmtc0	t2,C0_SR
*/
		j	ra		# return to caller
		
		.set noreorder
		nop;nop;nop;nop

END(sb1250_l2cache_init1)

#ifdef RUN_FROM_K0

LEAF(to_kseg0)
        li      v0, ~K1BASE
        and     ra, ra, v0
        li      v0, K0BASE
        or      ra, ra, v0
        jr      ra;
        nop
END(to_kseg0)    

#endif
