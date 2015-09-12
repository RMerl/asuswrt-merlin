/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Wafer ID bit definitions			File: sb1250_wid.h
    *  
    *  Some preproduction BCM1250 samples use the wafer ID (WID) bits
    *  in the system_revision register in the SCD to determine which
    *  portions of the L1 and L2 caches are usable.
    * 
    *  This file describes the WID register layout.
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


#ifndef _SB1250_WID_H
#define _SB1250_WID_H

#include "sb1250_defs.h"

/*
 * To make things easier to work with, we'll assume that the
 * WID bits have been shifted from their normal home
 * in scd_system_revision[63:32] to bits [31..0].  
 *
 * That is, we've already shifted right by S_SYS_WID
 */

#define S_WID_BIN             0
#define M_WID_BIN             _SB_MAKEMASK(3,S_WID_BIN)
#define V_WID_BIN(x)          _SB_MAKEVALUE(x,S_WID_BIN)
#define G_WID_BIN(x)          _SB_GETVALUE(x,S_WID_BIN,M_WID_BIN)

                                        /* CPUs  L1I    L1D    L2   */
#define K_WID_BIN_2CPU_FI_1D_H2	0	/*  2    full   1/4    1/2  */
#define K_WID_BIN_2CPU_FI_FD_F2	1	/*  2    full   full   full */
#define K_WID_BIN_2CPU_FI_FD_H2 2 	/*  2    full   full   1/2  */
#define K_WID_BIN_2CPU_3I_3D_F2 3       /*  2    3/4    3/4    full */
#define K_WID_BIN_2CPU_3I_3D_H2 4	/*  2    3/4    3/4    1/2  */
#define K_WID_BIN_1CPU_FI_FD_F2 5	/*  1    full   full   full */
#define K_WID_BIN_1CPU_FI_FD_H2 6       /*  1    full   full   1/2  */
#define K_WID_BIN_2CPU_1I_1D_Q2 7       /*  2    1/4    1/4    1/4  */

/*
 * '1' bits in this mask represent bins with only one CPU
 */

#define M_WID_BIN_1CPU (_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_F2) | \
                        _SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_H2))


/*
 * '1' bits in this mask represent bins with a good L2 
 */

#define M_WID_BIN_F2 (_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_F2) | \
                      _SB_MAKEMASK1(K_WID_BIN_2CPU_3I_3D_F2) | \
                      _SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_F2))

/*
 * '1' bits in this mask represent bins with 1/2 L2
 */

#define M_WID_BIN_H2   (_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_1D_H2) | \
                        _SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_H2) | \
                        _SB_MAKEMASK1(K_WID_BIN_2CPU_3I_3D_H2) | \
                        _SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_H2) )

/*
 * '1' bits in this mask represent bins with 1/4 L2
 */

#define M_WID_BIN_Q2   (_SB_MAKEMASK1(K_WID_BIN_2CPU_1I_1D_Q2))

/*
 * '1' bits in this mask represent bins with 3/4 L1 
 */

#define M_WID_BIN_3ID  (_SB_MAKEMASK1(K_WID_BIN_2CPU_3I_3D_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_2CPU_3I_3D_H2))

/*
 * '1' bits in this mask represent bins with a full L1I 
 */

#define M_WID_BIN_FI   (_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_1D_H2) | \
			_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_H2) | \
			_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_H2))
			
/*
 * '1' bits in this mask represent bins with a full L1D
 */

#define M_WID_BIN_FD   (_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_H2) | \
			_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_H2))

/*
 * '1' bits in this mask represent bins with a full L1 (both I and D)
 */

#define M_WID_BIN_FID  (_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_2CPU_FI_FD_H2) | \
			_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_F2) | \
			_SB_MAKEMASK1(K_WID_BIN_1CPU_FI_FD_H2))

#define S_WID_L2QTR           3
#define M_WID_L2QTR           _SB_MAKEMASK(2,S_WID_L2QTR)
#define V_WID_L2QTR(x)        _SB_MAKEVALUE(x,S_WID_L2QTR)
#define G_WID_L2QTR(x)        _SB_GETVALUE(x,S_WID_L2QTR,M_WID_L2QTR)

#define M_WID_L2HALF	      _SB_MAKEMASK1(4)

#define S_WID_CPU0_L1I        5
#define M_WID_CPU0_L1I        _SB_MAKEMASK(2,S_WID_CPU0_L1I)
#define V_WID_CPU0_L1I(x)     _SB_MAKEVALUE(x,S_WID_CPU0_L1I)
#define G_WID_CPU0_L1I(x)     _SB_GETVALUE(x,S_WID_CPU0_L1I,M_WID_CPU0_L1I)

#define S_WID_CPU0_L1D        7
#define M_WID_CPU0_L1D        _SB_MAKEMASK(2,S_WID_CPU0_L1D)
#define V_WID_CPU0_L1D(x)     _SB_MAKEVALUE(x,S_WID_CPU0_L1D)
#define G_WID_CPU0_L1D(x)     _SB_GETVALUE(x,S_WID_CPU0_L1D,M_WID_CPU0_L1D)

#define S_WID_CPU1_L1I        9
#define M_WID_CPU1_L1I        _SB_MAKEMASK(2,S_WID_CPU1_L1I)
#define V_WID_CPU1_L1I(x)     _SB_MAKEVALUE(x,S_WID_CPU1_L1I)
#define G_WID_CPU1_L1I(x)     _SB_GETVALUE(x,S_WID_CPU1_L1I,M_WID_CPU1_L1I)

#define S_WID_CPU1_L1D        11
#define M_WID_CPU1_L1D        _SB_MAKEMASK(2,S_WID_CPU1_L1D)
#define V_WID_CPU1_L1D(x)     _SB_MAKEVALUE(x,S_WID_CPU1_L1D)
#define G_WID_CPU1_L1D(x)     _SB_GETVALUE(x,S_WID_CPU1_L1D,M_WID_CPU1_L1D)

/*
 * The macros below assume that the CPU bits have been shifted into the
 * low-order 4 bits.
 */

#define S_WID_CPUX_L1I        0
#define M_WID_CPUX_L1I        _SB_MAKEMASK(2,S_WID_CPUX_L1I)
#define V_WID_CPUX_L1I(x)     _SB_MAKEVALUE(x,S_WID_CPUX_L1I)
#define G_WID_CPUX_L1I(x)     _SB_GETVALUE(x,S_WID_CPUX_L1I,M_WID_CPUX_L1I)

#define S_WID_CPUX_L1D        2
#define M_WID_CPUX_L1D        _SB_MAKEMASK(2,S_WID_CPUX_L1D)
#define V_WID_CPUX_L1D(x)     _SB_MAKEVALUE(x,S_WID_CPUX_L1D)
#define G_WID_CPUX_L1D(x)     _SB_GETVALUE(x,S_WID_CPUX_L1D,M_WID_CPUX_L1D)

#define S_WID_CPU0	      5
#define S_WID_CPU1	      9

#define S_WID_WAFERID        13
#define M_WID_WAFERID        _SB_MAKEMASK(5,S_WID_WAFERID)
#define V_WID_WAFERID(x)     _SB_MAKEVALUE(x,S_WID_WAFERID)
#define G_WID_WAFERID(x)     _SB_GETVALUE(x,S_WID_WAFERID,M_WID_WAFERID)

#define S_WID_LOTID        18
#define M_WID_LOTID        _SB_MAKEMASK(14,S_WID_LOTID)
#define V_WID_LOTID(x)     _SB_MAKEVALUE(x,S_WID_LOTID)
#define G_WID_LOTID(x)     _SB_GETVALUE(x,S_WID_LOTID,M_WID_LOTID)

/*
 * Now, to make things even more confusing, the fuses on the chip
 * don't exactly correspond to the bits in the register. The mask 
 * below represents bits that need to be swapped with the ones to
 * their left.  So, if bit 10 is set, swap bits 10 and 11
 */

#define M_WID_SWAPBITS	(_SB_MAKEMASK1(2) | _SB_MAKEMASK1(4) | _SB_MAKEMASK1(10) | \
                         _SB_MAKEMASK1(20) | _SB_MAKEMASK1(18) | _SB_MAKEMASK1(26) )

#ifdef __ASSEMBLER__
#define WID_UNCONVOLUTE(wid,t1,t2,t3) \
       li    t1,M_WID_SWAPBITS ; \
       and   t1,t1,wid ; \
       sll   t1,t1,1 ; \
       li    t2,(M_WID_SWAPBITS << 1); \
       and   t2,t2,wid ; \
       srl   t2,t2,1 ; \
       li    t3, ~((M_WID_SWAPBITS | (M_WID_SWAPBITS << 1))) ; \
       and   wid,wid,t3 ; \
       or    wid,wid,t1 ; \
       or    wid,wid,t2
#else
#define WID_UNCONVOLUTE(wid) \
     (((wid) & ~((M_WID_SWAPBITS | (M_WID_SWAPBITS << 1)))) | \
      (((wid) & M_WID_SWAPBITS) << 1) |  \
      (((wid) & (M_WID_SWAPBITS<<1)) >> 1))
#endif



#endif
