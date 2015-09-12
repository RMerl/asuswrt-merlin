/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Physical memory peek/poke routines	File: lib_physio.h
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


#ifndef _LIB_PHYSIO_H
#define _LIB_PHYSIO_H

//#ifndef _SB_MIPS_H
//#include "sbmips.h"
//#endif

/*
 * If __long64 we can do this via macros.  Otherwise, call
 * the magic functions.
 */

#if __long64

typedef long physaddr_t;
#define _PHYSADDR_T_DEFINED_

/*#define __UNCADDRX(x) PHYS_TO_XKSEG_UNCACHED(x)*/
#define __UNCADDRX(x) (((physaddr_t)(x))|0x9000000000000000)


#define phys_write8(a,b) *((volatile uint8_t *) __UNCADDRX(a)) = (b)
#define phys_write16(a,b) *((volatile uint16_t *) __UNCADDRX(a)) = (b)
#define phys_write32(a,b) *((volatile uint32_t *) __UNCADDRX(a)) = (b)
#define phys_write64(a,b) *((volatile uint64_t *) __UNCADDRX(a)) = (b)
#define phys_read8(a) *((volatile uint8_t *) __UNCADDRX(a))
#define phys_read16(a) *((volatile uint16_t *) __UNCADDRX(a))
#define phys_read32(a) *((volatile uint32_t *) __UNCADDRX(a))
#define phys_read64(a) *((volatile uint64_t *) __UNCADDRX(a))

#else	/* not __long64 */

#ifdef _MIPSREGS32_

#include "addrspace.h"

typedef long physaddr_t;	/* 32-bit-only processors can't have >32bit pa's */

#if defined(__MIPSEB) && defined(_MIPSEB_DATA_INVARIANT_)
/*
 * Most of the time, address bits matter. If endianness is
 * implemented by swapping address lines on the way out the CPU,
 * defeat this mechanism.
 */
#ifdef	BCMHND74K

#define phys_write8(a,b) do { *((volatile uint8_t *) PHYS_TO_K1(((physaddr_t)(a))^7)) = (b); } while (0)
#define phys_write16(a,b) do { *((volatile uint16_t *) PHYS_TO_K1(((physaddr_t)(a))^6)) = (b); } while (0)
#define phys_write32(a,b) do { *((volatile uint32_t *) PHYS_TO_K1(((physaddr_t)(a))^4)) = (b); } while (0)
#define phys_read8(a) (*((volatile uint8_t *) PHYS_TO_K1(((physaddr_t)(a))^7)))
#define phys_read16(a) (*((volatile uint16_t *) PHYS_TO_K1(((physaddr_t)(a))^6)))
#define phys_read32(a) (*((volatile uint32_t *) PHYS_TO_K1(((physaddr_t)(a))^4)))

#else	/* Not 74k: bcm33xx */

#define phys_write8(a,b) do { *((volatile uint8_t *) PHYS_TO_K1(((physaddr_t)(a))^3)) = (b); } while (0)
#define phys_write16(a,b) do { *((volatile uint16_t *) PHYS_TO_K1(((physaddr_t)(a))^2)) = (b); } while (0)
#define phys_write32(a,b) do { *((volatile uint32_t *) PHYS_TO_K1((physaddr_t)(a))) = (b); } while (0)
#define phys_read8(a) (*((volatile uint8_t *) PHYS_TO_K1(((physaddr_t)(a))^3)))
#define phys_read16(a) (*((volatile uint16_t *) PHYS_TO_K1(((physaddr_t)(a))^2)))
#define phys_read32(a) (*((volatile uint32_t *) PHYS_TO_K1((physaddr_t)(a))))

#endif	/* BCMHND74K */

#else	/* sane endianness */

#define phys_write8(a,b) do { *((volatile uint8_t *) PHYS_TO_K1((physaddr_t)(a))) = (b); } while (0)
#define phys_write16(a,b) do { *((volatile uint16_t *) PHYS_TO_K1((physaddr_t)(a))) = (b); } while (0)
#define phys_write32(a,b) do { *((volatile uint32_t *) PHYS_TO_K1((physaddr_t)(a))) = (b); } while (0)
#define phys_read8(a) (*((volatile uint8_t *) PHYS_TO_K1((physaddr_t)(a))))
#define phys_read16(a) (*((volatile uint16_t *) PHYS_TO_K1((physaddr_t)(a))))
#define phys_read32(a) (*((volatile uint32_t *) PHYS_TO_K1((physaddr_t)(a))))

#endif

#else	/* not _MIPSREGS32_ */

typedef long long physaddr_t;	/* Otherwise, they might. */

extern void phys_write8(physaddr_t a,uint8_t b);
extern void phys_write16(physaddr_t a,uint16_t b);
extern void phys_write32(physaddr_t a,uint32_t b);
extern void phys_write64(physaddr_t a,uint64_t b);
extern uint8_t phys_read8(physaddr_t a);
extern uint16_t phys_read16(physaddr_t a);
extern uint32_t phys_read32(physaddr_t a);
extern uint64_t phys_read64(physaddr_t a);

#endif

#endif

#endif
