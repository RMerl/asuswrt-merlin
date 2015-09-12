/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Physical memory peek/poke routines	File: lib_physio.S
    *  
    *  Little stub routines to allow access to arbitrary physical
    *  addresses.  In most cases this should not be needed, as
    *  many physical addresses are within kseg1, but this handles
    *  the cases that are not automagically, so we don't need
    *  to mess up the code with icky macros and such.
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


#include "sbmips.h"
#include "mipsmacros.h"
#include "cpu_config.h"		/* for definition of HAZARD */


/*  *********************************************************************
    *  PHYSOP(inst,a)
    *  
    *  Macro to construct code for doing the physical I/O
    *  We try to avoid messing with KX or doing 64-bit stuff
    *  unless necessary.
    *  
    *  Input parameters: 
    *  	   INST - instruction name to run
    *  	   A - register containing arg or return value
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */



#ifdef _MIPSREGS32_
#define PHYSOP(INST,A) \
		or	a0,a0,K1BASE    ; \
		INST	A,0(a0)         ; \
		j	ra              ;
	
#if defined(__MIPSEB) && defined(_MIPSEB_DATA_INVARIANT_)

#ifdef	BCMHND74K

#define PHYSOP8(INST,A) \
		or	a0,a0,K1BASE    ; \
		xori	a0,a0,7		; \
		INST	A,0(a0)         ; \
		j	ra              ;
#define PHYSOP16(INST,A) \
		or	a0,a0,K1BASE    ; \
		xori	a0,a0,6		; \
		INST	A,0(a0)         ; \
		j	ra              ;
#define PHYSOP32(INST,A) \
		or	a0,a0,K1BASE    ; \
		xori	a0,a0,4		; \
		INST	A,0(a0)         ; \
		j	ra              ;

#else	/* Not 74K, bcm33xx */

#define PHYSOP8(INST,A) \
		or	a0,a0,K1BASE    ; \
		xori	a0,a0,3		; \
		INST	A,0(a0)         ; \
		j	ra              ;
#define PHYSOP16(INST,A) \
		or	a0,a0,K1BASE    ; \
		xori	a0,a0,2		; \
		INST	A,0(a0)         ; \
		j	ra              ;
#define PHYSOP32(INST,A) PHYSOP(INST,A)

#endif	/* BCMHND74K */

#else	/* !(BE && DATA_INV) */
#define PHYSOP8(INST,A) PHYSOP(INST,A)
#define PHYSOP16(INST,A) PHYSOP(INST,A)
#define PHYSOP32(INST,A) PHYSOP(INST,A)
#endif	/* BE && DATA_INV */
#define PHYSOP64(INST,A) PHYSOP(INST,A)
#else	/* !REGS32 */
#define PHYSOP(INST,A) \
		lui	t0,0x2000       ; \
		bgeu	a0,t0,1f        ; \
		or	a0,a0,K1BASE    ; \
		INST	A,0(a0)         ; \
		j	ra              ; \
1:		lui	t0,0x9000       ; \
		dsll	t0,t0,32        ; \
		or	a0,a0,t0        ; \
		mfc0	t1,C0_SR        ; \
		and	t0,t1,M_SR_KX   ; \
		beq	t0,zero,1f      ; \
		INST	A,0(a0)         ; \
		j	ra              ; \
1:		or	t0,t1,M_SR_KX   ; \
		mtc0	t0,C0_SR        ; \
		HAZARD                  ; \
		INST	A,0(a0)         ; \
		mtc0	t1,C0_SR        ; \
		HAZARD                  ; \
		j	ra              ;
#define PHYSOP8(INST,A) PHYSOP(INST,A)
#define PHYSOP16(INST,A) PHYSOP(INST,A)
#define PHYSOP32(INST,A) PHYSOP(INST,A)
#define PHYSOP64(INST,A) PHYSOP(INST,A)
#endif	/* REGS32 */

/*  *********************************************************************
    *  phys_read8 - read 8-bit bytes
    ********************************************************************* */


LEAF(phys_read8)
PHYSOP8(lbu,v0)
END(phys_read8)

/*  *********************************************************************
    *  phys_read16 - read 16-bit shorts
    ********************************************************************* */

LEAF(phys_read16)
PHYSOP16(lh,v0)
END(phys_read16)

/*  *********************************************************************
    *  phys_read32 - read 32-bit ints
    ********************************************************************* */

LEAF(phys_read32)
PHYSOP32(lw,v0)
END(phys_read32)

/*  *********************************************************************
    *  phys_read64 - read 64-bit longs
    ********************************************************************* */

LEAF(phys_read64)
PHYSOP64(ld,v0)
END(phys_read64)

/*  *********************************************************************
    *  phys_write8 - write 8-bit bytes
    ********************************************************************* */

LEAF(phys_write8)
PHYSOP8(sb,a1)
END(phys_write8)

/*  *********************************************************************
    *  phys_write16 - write 16-bit shorts
    ********************************************************************* */

LEAF(phys_write16)
PHYSOP16(sh,a1)
END(phys_write16)

/*  *********************************************************************
    *  phys_write32 - write 32-bit longs
    ********************************************************************* */

LEAF(phys_write32)
PHYSOP32(sw,a1)
END(phys_write32)

/*  *********************************************************************
    *  phys_write64 - write 64-bit longs
    ********************************************************************* */

LEAF(phys_write64)
PHYSOP64(sd,a1)
END(phys_write64)


/*  *********************************************************************
    *  End
    ********************************************************************* */
