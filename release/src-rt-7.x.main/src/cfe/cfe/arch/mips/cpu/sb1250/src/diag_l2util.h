/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  L2 Cache Diagnostic			File: diag_l2util.h
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

/* l2 physical layout */
#define L2_BLOCK_ROW_SHIFT	14
#define L2_BLOCK_SHIFT		13
#define L2_LINE_SHIFT		5
#define L2_LINE_ADDRESS_BITS	17
#define L2_BLOCKS_PER_ROW	2
#define L2_BLOCK_ROWS		8
#define L2_REDUNDANT_LINES_PER_BLOCK 2
#define L2_LINES_PER_BLOCK	256
#define L2_INDEX_MASK		0x1fe0

/* management interface */
#define L2_RAM_BASE_ADDR	0x00D0180000

#if defined(__ASSEMBLER__) 

#define L2_MGMT_BASE		0x00d0000000
#define L2_NWAYS		4
#define L2_MGMT_WAY_SHIFT	17

#else 

#define L2_MGMT_BASE		0x00d0000000L
#define L2_RAM_BASE_ADDR	0x00D0180000
#define L2_RAM_ADDR(W,O)	(L2_RAM_BASE_ADDR + ((W)<<17) + ((O)&0x1FFFF))
#define L2_MGMT_INDEX_SHIFT	5L
#define L2_NROWS		4096
#define L2_MGMT_ROW_SHIFT	L2_MGMT_INDEX_SHIFT
#define L2_MGMT_WAY_SHIFT	17L
#define L2_MGMT_DIRTY_SHIFT	19
#define L2_MGMT_VALID_SHIFT     20


/* disable ways of the l2 cache */
#define L2_WAY_DISABLE_NONE	0x0010041F00L
#define L2_NWAYS		4L
#define L2_WAY_SHIFT		8L
#define L2_ENABLE_ALL_WAYS	(*(volatile int *)\
				 (XKPHYS_PtoUV(L2_WAY_DISABLE_NONE)) = 0)
#define L2_ENABLE_ONLY1_WAY(x)	(*(volatile int *)\
				 (XKPHYS_PtoUV(L2_WAY_DISABLE_NONE & \
                                  (1L << (((x) & 0x3L)+L2_WAY_SHIFT)))) = 0) 

#define L2_DISABLE_ONLY1_WAY(x)	(*(volatile int *)\
				 (XKPHYS_PtoUV(L2_WAY_DISABLE_NONE & \
		                 (((~(1L << ((x) & 0x3L))) & 0xfL) \
				  << L2_WAY_SHIFT))) = 0)

typedef unsigned long long addr_t;

/* this structure is used to record lines with errors */
typedef struct {
  int block;
  int block_row;
  int line;
} l2_line_error_desc_t;

extern int errors_per_block[L2_BLOCK_ROWS * L2_BLOCKS_PER_ROW];

#endif
