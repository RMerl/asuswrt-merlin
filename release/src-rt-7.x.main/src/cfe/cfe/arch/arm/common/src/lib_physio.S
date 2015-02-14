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
    *  Author:
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


#include <arminc.h>
#include <armmacros.h>


/*  *********************************************************************
    *  PHYSOP(inst,a)
    *  
    *  Macro to construct code for doing the physical I/O
    *  We try to avoid messing with KX or doing 64-bit stuff
    *  unless necessary.
    *  
    *  r0: addr / return value
    *  r1: value
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

/*  *********************************************************************
    *  phys_read8 - read 8-bit bytes
    ********************************************************************* */

FUNC(phys_read8)
	ldrb  r1, [r0]
	mov  r0, r1
	mov  pc, lr
END(phys_read8)

/*  *********************************************************************
    *  phys_read16 - read 16-bit shorts
    ********************************************************************* */

FUNC(phys_read16)
	ldrh  r1, [r0]
	mov  r0, r1
	mov  pc, lr
END(phys_read16)

/*  *********************************************************************
    *  phys_read32 - read 32-bit ints
    ********************************************************************* */

FUNC(phys_read32)
	ldr  r1, [r0]
	mov  r0, r1
	mov  pc, lr
END(phys_read32)

/*  *********************************************************************
    *  phys_read64 - read 64-bit longs
    ********************************************************************* */

FUNC(phys_read64)
	ldr  r1, [r0]
	mov  r0, r1
	mov  pc, lr
END(phys_read64)

/*  *********************************************************************
    *  phys_write8 - write 8-bit bytes
    ********************************************************************* */

FUNC(phys_write8)
	/* r0: addr, r1: value */
	strb r1, [r0]
	mov  pc, lr
END(phys_write8)

/*  *********************************************************************
    *  phys_write16 - write 16-bit shorts
    ********************************************************************* */

FUNC(phys_write16)
	/* r0: addr, r1: value */
	strh r1, [r0]
	mov  pc, lr
END(phys_write16)

/*  *********************************************************************
    *  phys_write32 - write 32-bit longs
    ********************************************************************* */

FUNC(phys_write32)
	/* r0: addr, r1: value */
	str r1, [r0]
	mov  pc, lr
END(phys_write32)

/*  *********************************************************************
    *  phys_write64 - write 64-bit longs
    ********************************************************************* */
FUNC(phys_write64)
	/* r0: addr, r1: value */
	str r1, [r0]
	mov  pc, lr
END(phys_write64)


/*  *********************************************************************
    *  End
    ********************************************************************* */
