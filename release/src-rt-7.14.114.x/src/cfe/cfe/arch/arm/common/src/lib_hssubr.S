/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Hyperspace Subroutines			File: lib_hssubr.S
    *  
    *  Little stub routines to allow access to KXSEG from 32-bit progs.
    *  
    *  Author:  Mitch Lichtenberg (mitch@sibyte.com)
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
#include "cpu_config.h"		/* for definition of HAZARD */


/*  *********************************************************************
    *  hs_read8 - read 8-bit bytes
    ********************************************************************* */


LEAF(hs_read8)
		/* Not implement */
		mov		pc,lr
END(hs_read8)

/*  *********************************************************************
    *  hs_read16 - read 16-bit shorts
    ********************************************************************* */

LEAF(hs_read16)
		/* Not implement */
		mov		pc,lr
END(hs_read16)

/*  *********************************************************************
    *  hs_read32 - read 32-bit ints
    ********************************************************************* */

LEAF(hs_read32)
		/* Not implement */
		mov		pc,lr
END(hs_read32)

/*  *********************************************************************
    *  hs_read64 - read 64-bit longs
    ********************************************************************* */

LEAF(hs_read64)
		/* Not implement */
		mov		pc,lr
END(hs_read64)

/*  *********************************************************************
    *  hs_write8 - write 8-bit bytes
    ********************************************************************* */

LEAF(hs_write8)
		/* Not implement */
		mov		pc,lr
END(hs_write8)

/*  *********************************************************************
    *  hs_write16 - write 16-bit shorts
    ********************************************************************* */

LEAF(hs_write16)
		/* Not implement */
		mov		pc,lr
END(hs_write16)

/*  *********************************************************************
    *  hs_write32 - write 32-bit longs
    ********************************************************************* */

LEAF(hs_write32)
		/* Not implement */
		mov		pc,lr
END(hs_write32)

/*  *********************************************************************
    *  hs_write64 - write 64-bit longs
    ********************************************************************* */

LEAF(hs_write64)
		/* Not implement */
		mov		pc,lr
END(hs_write64)


/*  *********************************************************************
    *  End
    ********************************************************************* */
