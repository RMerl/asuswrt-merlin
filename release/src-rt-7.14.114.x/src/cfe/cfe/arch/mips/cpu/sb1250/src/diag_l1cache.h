/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  L2 Cache Diagnostic			File: diag_l1cache.h
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

#define L1C_OP_IDXINVAL     0
#define L1C_OP_IDXLOADTAG   1
#define L1C_OP_IDXLOADDATA  1	
#define L1C_OP_IDXSTORETAG  2
#define L1C_OP_IDXSTOREDATA 2
#define L1C_OP_IMPLRSVD     3
#define L1C_OP_HITINVAL     4
#define L1C_OP_FILL         5
#define L1C_OP_HIT_WB_INVAL 5	
#define L1C_OP_HITWRITEBACK 6
#define L1C_OP_FETCHLOCK    7

#define L1C_I		    0
#define L1C_D		    1
#define L1CINDDATA_I	    2	/* for index store/load data */
#define L1CINDDATA_D	    3	

#define L1CACHEOP(cachename,op) ((cachename) | ((op) << 2))
	
#define UNMAPPED_UNCACHED_ADDR	0xa0000000
#define UNMAPPED_CACHED_ADDR	0x80000000

/* masks and field values for taglo and taghi registers */
#define DTAGLO_PTAG1_SHIFT	_SB_MAKE64(26)	/* Phy addr bits [39:26] */
#define DTAGLO_PTAG1_MASK	_SB_MAKEMASK(14,DTAGLO_PTAG1_SHIFT)
#define DTAGLO_PTAG0_SHIFT	_SB_MAKE64(13)	/* Phy addr bits [25:13] */
#define DTAGLO_PTAG0_MASK	_SB_MAKEMASK(13,DTAGLO_PTAG0_SHIFT)
#define DTAGLO_PTAG_SHIFT	_SB_MAKE64(13)	/* Phy addr bits [39:12] */
#define DTAGLO_PTAG_MASK	_SB_MAKEMASK(27,DTAGLO_PTAG0_SHIFT)
#define DTAGLO_P1_SHIFT		_SB_MAKE64(11)	/* parity bits for PTag1 */
#define DTAGLO_P0_SHIFT		_SB_MAKE64(10)	/* parity bits for PTag0 */

#define DTAGHI_STATE_SHIFT	_SB_MAKE64(28)	/* state bits */
#define DTAGHI_STATE_MASK	_SB_MAKEMASK(2,DTAGHI_STATE_SHIFT)
#define DTAGHI_COH_SHIFT	_SB_MAKE64(27)
#define DTAGHI_CHECK_SHIFT	_SB_MAKE64(25)
#define DTAGHI_CHECK_MASK	_SB_MAKEMASK(2,DTAGHI_CHECK_SHIFT)
#define DTAGHI_EXTNC_SHIFT	_SB_MAKE64(24)
#define DTAGHI_STREAM_SHIFT	_SB_MAKE64(23)	/* Stream bit */
#define DTAGHI_LU_SHIFT		_SB_MAKE64(22)	/* LU bit */
#define DTAGHI_LRU_SHIFT	_SB_MAKE64(14)	/* LRU bits */
#define DTAGHI_LRU_MASK		_SB_MAKEMASK(8,DTAGHI_LRU_SHIFT)

/* TAGHI state bits */
#define DTAGHI_ST_INVALID	00
#define DTAGHI_ST_SHARED	01
#define DTAGHI_ST_EXC_CLEAN	02
#define DTAGHI_ST_EXC_DIRTY	03

/* I Tag lo bits for  */
#define ITAGLO_REGION_SHIFT	_SB_MAKE64(62)
#define ITAGLO_REGION_MASK	_SB_MAKEMASK(2,ITAGLO_REGION_SHIFT)
#ifdef RTL
#define ITAGLO_VTAG1_SHIFT	_SB_MAKE64(24)
#define ITAGLO_VTAG1_MASK	_SB_MAKEMASK(20,ITAGLO_VTAG1_SHIFT)
#else
#define ITAGLO_VTAG1_SHIFT	_SB_MAKE64(25)
#define ITAGLO_VTAG1_MASK	_SB_MAKEMASK(19,ITAGLO_VTAG1_SHIFT)
#endif
#ifdef RTL
#define ITAGLO_VTAG0_SHIFT	_SB_MAKE64(13)
#define ITAGLO_VTAG0_MASK	_SB_MAKEMASK(11,ITAGLO_VTAG0_SHIFT)
#else
#define ITAGLO_VTAG0_SHIFT	_SB_MAKE64(13)
#define ITAGLO_VTAG0_MASK	_SB_MAKEMASK(12,ITAGLO_VTAG0_SHIFT)
#endif
#define ITAGLO_P1_SHIFT		_SB_MAKE64(11)
#define ITAGLO_P0_SHIFT		_SB_MAKE64(10)
#define ITAGLO_GLOBAL_SHIFT	_SB_MAKE64(8)
#define ITAGLO_ASID_SHIFT	_SB_MAKE64(0)
#define ITAGLO_ASID_MASK	_SB_MAKEMASK(8,ITAGLO_ASID_SHIFT)

#define ITAGHI_VALID_SHIFT	_SB_MAKE64(29)
#define ITAGHI_PARITY_SHIFT	_SB_MAKE64(27)
#define ITAGHI_LRU_SHIFT	_SB_MAKE64(14)
#define ITAGHI_LRU_MASK		_SB_MAKEMASK(8,ITAGHI_LRU_SHIFT)
#define ITAGHI_LU_SHIFT		_SB_MAKE64(22)

#define DTAG_LO_PAT(ptag1,ptag0,parity_tag1,parity_tag0) \
			((_SB_MAKEVALUE(ptag1,DTAGLO_PTAG1_SHIFT) & \
			  DTAGLO_PTAG1_MASK) |\
			 (_SB_MAKEVALUE(ptag0,DTAGLO_PTAG0_SHIFT) & \
			  DTAGLO_PTAG0_MASK) |\
			 _SB_MAKEVALUE(parity_tag1,DTAGLO_P1_SHIFT) |\
			 _SB_MAKEVALUE(parity_tag0,DTAGLO_P0_SHIFT))
#define DTAG_HI_PAT(state,coh,check,enc,stream,lu,lru)			\
			((_SB_MAKEVALUE(state, DTAGHI_STATE_SHIFT)&	\
			  DTAGHI_STATE_MASK)|				\
			 (_SB_MAKEVALUE(coh, DTAGHI_COH_SHIFT)) |	\
			 (_SB_MAKEVALUE(check, DTAGHI_CHECK_SHIFT) &	\
		          DTAGHI_CHECK_MASK) |				\
			 _SB_MAKEVALUE(enc,DTAGHI_EXTNC_SHIFT) |	\
			 _SB_MAKEVALUE(stream,DTAGHI_STREAM_SHIFT) |	\
			 _SB_MAKEVALUE(lu,DTAGHI_LU_SHIFT) |		\
			 (_SB_MAKEVALUE(lru,DTAGHI_LRU_SHIFT) &		\
			  DTAGHI_LRU_MASK))

/* L1 characteristicts */
#define MAX_WAYS	4
#define MAX_LINES	LINES_TO_TEST		
#define DWORDS_PER_LINE	4      
#define MAX_PATTERN	12
